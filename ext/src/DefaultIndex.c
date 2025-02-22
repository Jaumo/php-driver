/**
 * Copyright 2015-2017 DataStax, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "php_driver.h"
#include "php_driver_types.h"
#include "util/result.h"
#include "util/ref.h"
#include "util/types.h"

#include "DefaultIndex.h"

zend_class_entry *php_driver_default_index_ce = NULL;

zval
php_driver_create_index(php_driver_ref *schema,
                           const CassIndexMeta *meta)
{
  zval result;
  php_driver_index *index;
  const char *name;
  size_t name_length;

  ZVAL_UNDEF(&(result));


  object_init_ex(&(result), php_driver_default_index_ce);

  index = PHP_DRIVER_GET_INDEX(&(result));
  index->meta   = meta;
  index->schema = php_driver_add_ref(schema);

  cass_index_meta_name(meta, &name, &name_length);

  ZVAL_STRINGL(&(index->name), name, name_length);

  return result;
}

PHP_METHOD(DefaultIndex, name)
{
  php_driver_index *self;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  RETURN_ZVAL(&(self->name), 1, 0);
}

PHP_METHOD(DefaultIndex, target)
{
  php_driver_index *self;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->target)) {
    const char *target;
    size_t target_length;
    cass_index_meta_target(self->meta, &target, &target_length);

    ZVAL_STRINGL(&(self->target), target, target_length);
  }

  RETURN_ZVAL(&(self->target), 1, 0);
}

PHP_METHOD(DefaultIndex, kind)
{
  php_driver_index *self;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->kind)) {

    switch (cass_index_meta_type(self->meta)) {
      case CASS_INDEX_TYPE_KEYS:
        ZVAL_STRING(&(self->kind), "keys");
        break;
      case CASS_INDEX_TYPE_CUSTOM:
        ZVAL_STRING(&(self->kind), "custom");
        break;
      case CASS_INDEX_TYPE_COMPOSITES:
        ZVAL_STRING(&(self->kind), "composites");
        break;
      default:
        ZVAL_STRING(&(self->kind), "unknown");
        break;
    }
  }

  RETURN_ZVAL(&(self->kind), 1, 0);
}

void php_driver_index_build_option(php_driver_index *index)
{
  const CassValue* options;


  array_init(&(index->options));
  options = cass_index_meta_options(index->meta);
  if (options) {
    CassIterator* iterator = cass_iterator_from_map(options);
    while (cass_iterator_next(iterator)) {
      const char* key_str;
      size_t key_str_length;
      const char* value_str;
      size_t value_str_length;
      const CassValue* key = cass_iterator_get_map_key(iterator);
      const CassValue* value = cass_iterator_get_map_value(iterator);

      cass_value_get_string(key, &key_str, &key_str_length);
      cass_value_get_string(value, &value_str, &value_str_length);
      add_assoc_stringl_ex(&(index->options), key_str, key_str_length, value_str, value_str_length);
    }
  }
}

PHP_METHOD(DefaultIndex, option)
{
  char *name;
  size_t name_len;
  php_driver_index *self;
  zval* result;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                            &name, &name_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->options)) {
    php_driver_index_build_option(self);
  }

  if (CASS_ZEND_HASH_FIND(Z_ARRVAL(self->options),
                          name, name_len + 1,
                          result)) {
    RETURN_ZVAL(result, 1, 0);
  }
  RETURN_FALSE;
}

PHP_METHOD(DefaultIndex, options)
{
  php_driver_index *self;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->options)) {
    php_driver_index_build_option(self);
  }

  RETURN_ZVAL(&(self->options), 1, 0);
}

PHP_METHOD(DefaultIndex, className)
{
  php_driver_index *self;
  zval* result;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->options)) {
    php_driver_index_build_option(self);
  }

  if (CASS_ZEND_HASH_FIND(Z_ARRVAL(self->options),
                          "class_name", sizeof("class_name"),
                          result)) {
    RETURN_ZVAL(result, 1, 0);
  }
  RETURN_FALSE;
}

PHP_METHOD(DefaultIndex, isCustom)
{
  php_driver_index *self;
  int is_custom;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_INDEX(getThis());
  if (Z_ISUNDEF(self->options)) {
    php_driver_index_build_option(self);
  }

  is_custom = zend_hash_str_exists(Z_ARRVAL(self->options),"class_name", strlen("class_name"));
  RETURN_BOOL(is_custom);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_name, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_default_index_methods[] = {
  PHP_ME(DefaultIndex, name, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, kind, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, target, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, option, arginfo_name, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, options, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, className, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultIndex, isCustom, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_default_index_handlers;

static HashTable *
php_driver_type_default_index_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static int
php_driver_default_index_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_default_index_free(zend_object *object)
{
  php_driver_index *self = php_driver_index_object_fetch(object);;

  CASS_ZVAL_MAYBE_DESTROY(self->name);
  CASS_ZVAL_MAYBE_DESTROY(self->kind);
  CASS_ZVAL_MAYBE_DESTROY(self->target);
  CASS_ZVAL_MAYBE_DESTROY(self->options);

  if (self->schema) {
    php_driver_del_ref(&self->schema);
    self->schema = NULL;
  }
  self->meta = NULL;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_default_index_new(zend_class_entry *ce)
{
  php_driver_index *self =
      CASS_ZEND_OBJECT_ECALLOC(index, ce);

  ZVAL_UNDEF(&(self->name));
  ZVAL_UNDEF(&(self->kind));
  ZVAL_UNDEF(&(self->target));
  ZVAL_UNDEF(&(self->options));

  self->schema = NULL;
  self->meta = NULL;

  CASS_ZEND_OBJECT_INIT_EX(index, default_index, self, ce);
}

void php_driver_define_DefaultIndex()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\DefaultIndex", php_driver_default_index_methods);
  php_driver_default_index_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_default_index_ce, 1, php_driver_index_ce);
  php_driver_default_index_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_default_index_ce->create_object = php_driver_default_index_new;

  memcpy(&php_driver_default_index_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_default_index_handlers.get_gc          = php_driver_type_default_index_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_default_index_handlers, php_driver_default_index_compare);
  php_driver_default_index_handlers.clone_obj = NULL;
}
