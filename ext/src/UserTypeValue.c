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
#include "util/collections.h"
#include "util/hash.h"
#include "util/types.h"

#include "src/Type/UserType.h"
#include "src/UserTypeValue.h"

zend_class_entry *php_driver_user_type_value_ce = NULL;

void
php_driver_user_type_value_set(php_driver_user_type_value *user_type_value,
                                  const char *name, size_t name_length,
                                  zval *object)
{
  zend_hash_str_update(&user_type_value->values, name, name_length, object);
  Z_TRY_ADDREF_P(object);
  user_type_value->dirty = 1;
}

static void
php_driver_user_type_value_populate(php_driver_user_type_value *user_type_value, zval *array)
{
  char *name;
  php_driver_type *type;
  zval *current;
  zval null;

  type = PHP_DRIVER_GET_TYPE(&(user_type_value->type));

  // avoid unused variable warning
  (void) current;

  CASS_ZEND_HASH_FOREACH_STR_KEY_VAL(&type->data.udt.types, name, current) {
    zval *value = NULL;
    size_t name_len = strlen(name);

    value = zend_hash_str_find(&user_type_value->values, name, name_len);

    if (value != NULL) {
      add_assoc_zval_ex(array, name, name_len, value);
      Z_TRY_ADDREF_P(value);
    } else {
      ZVAL_NULL(&null);
      add_assoc_zval_ex(array, name, name_len, &null);
      Z_TRY_ADDREF_P(&null);
    }
  } ZEND_HASH_FOREACH_END();
}

/* {{{ UserTypeValue::__construct(types) */
PHP_METHOD(UserTypeValue, __construct)
{
  php_driver_user_type_value *self;
  php_driver_type *type;
  HashTable *types;
  char *name;
  int index = 0;
  zval *current;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "h", &types) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  self->type = php_driver_type_user_type();
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  CASS_ZEND_HASH_FOREACH_STR_KEY_VAL(types, name, current) {
    zval *sub_type = current;
    zval scalar_type;

    if (!name) {
      zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                              "Argument %d is not a string", index + 1);
      return;
    }
    index++;

    if (Z_TYPE_P(sub_type) == IS_STRING) {
      CassValueType value_type;
      if (!php_driver_value_type(Z_STRVAL_P(sub_type), &value_type)) {
        return;
      }
      scalar_type = php_driver_type_scalar(value_type);
      if (!php_driver_type_user_type_add(type,
                                            name, strlen(name),
                                            &(scalar_type))) {
        return;
      }
    } else if (Z_TYPE_P(sub_type) == IS_OBJECT &&
               instanceof_function(Z_OBJCE_P(sub_type), php_driver_type_ce)) {
      if (!php_driver_type_validate(sub_type, "sub_type")) {
        return;
      }
      if (php_driver_type_user_type_add(type,
                                           name, strlen(name),
                                           sub_type)) {
        Z_ADDREF_P(sub_type);
      } else {
        return;
      }
    } else {
      INVALID_ARGUMENT(sub_type, "a string or an instance of " PHP_DRIVER_NAMESPACE "\\Type");
    }
  } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ UserTypeValue::type() */
PHP_METHOD(UserTypeValue, type)
{
  php_driver_user_type_value *self = PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  RETURN_ZVAL(&(self->type), 1, 0);
}

/* {{{ UserTypeValue::values() */
PHP_METHOD(UserTypeValue, values)
{
  php_driver_user_type_value *self = NULL;
  self = PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());

  array_init(return_value);
  php_driver_user_type_value_populate(self, return_value);
}
/* }}} */

/* {{{ UserTypeValue::set(name, mixed) */
PHP_METHOD(UserTypeValue, set)
{
  php_driver_user_type_value *self = NULL;
  php_driver_type *type;
  zval *sub_type;
  char *name;
  size_t name_length;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz",
                            &name, &name_length,
                            &value) == FAILURE)
    return;

  self = PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  if (!CASS_ZEND_HASH_FIND(&type->data.udt.types,
                           name, name_length + 1,
                           sub_type)) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Invalid name '%s'", name);
    return;
  }

  if (!php_driver_validate_object(value,
                                     sub_type)) {
    return;
  }

  php_driver_user_type_value_set(self, name, name_length, value);
}
/* }}} */

/* {{{ UserTypeValue::get(name) */
PHP_METHOD(UserTypeValue, get)
{
  php_driver_user_type_value *self = NULL;
  php_driver_type *type;
  zval *sub_type;
  char *name;
  size_t name_length;
  zval *value;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s",
                            &name, &name_length) == FAILURE)
    return;

  self = PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  type = PHP_DRIVER_GET_TYPE(&(self->type));

  if (!CASS_ZEND_HASH_FIND(&type->data.udt.types,
                           name, name_length + 1,
                           sub_type)) {
    zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                            "Invalid name '%s'", name);
    return;
  }

  if (CASS_ZEND_HASH_FIND(&self->values,
                          name, name_length + 1,
                          value)) {
    RETURN_ZVAL(value, 1, 0);
  }
}
/* }}} */

/* {{{ UserTypeValue::count() */
PHP_METHOD(UserTypeValue, count)
{
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  RETURN_LONG(zend_hash_num_elements(&type->data.udt.types));
}
/* }}} */

/* {{{ UserTypeValue::current() */
PHP_METHOD(UserTypeValue, current)
{
  zend_string *key;
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  if (zend_hash_get_current_key_ex(&type->data.udt.types, &key, ((void *) 0), &self->pos) == HASH_KEY_IS_STRING) {
    zval *value;
    if (CASS_ZEND_HASH_FIND(&self->values, key->val, key->len + 1, value)) {
      RETURN_ZVAL(value, 1, 0);
    }
  }
}
/* }}} */

/* {{{ UserTypeValue::key() */
PHP_METHOD(UserTypeValue, key)
{
  zend_string *key;
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  if (zend_hash_get_current_key_ex(&type->data.udt.types, &key, ((void *) 0), &self->pos) == HASH_KEY_IS_STRING) {
    RETURN_STR(key);
  }
}
/* }}} */

/* {{{ UserTypeValue::next() */
PHP_METHOD(UserTypeValue, next)
{
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  zend_hash_move_forward_ex(&type->data.udt.types, &self->pos);
}
/* }}} */

/* {{{ UserTypeValue::valid() */
PHP_METHOD(UserTypeValue, valid)
{
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  RETURN_BOOL(zend_hash_has_more_elements_ex(&type->data.udt.types, &self->pos) == SUCCESS);
}
/* }}} */

/* {{{ UserTypeValue::rewind() */
PHP_METHOD(UserTypeValue, rewind)
{
  php_driver_user_type_value *self =
      PHP_DRIVER_GET_USER_TYPE_VALUE(getThis());
  php_driver_type *type =
      PHP_DRIVER_GET_TYPE(&(self->type));
  zend_hash_internal_pointer_reset_ex(&type->data.udt.types, &self->pos);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, types)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_value, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_name, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_user_type_value_methods[] = {
  PHP_ME(UserTypeValue, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, values, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, set, arginfo_value, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, get, arginfo_name, ZEND_ACC_PUBLIC)
  /* Countable */
  PHP_ME(UserTypeValue, count, arginfo_none, ZEND_ACC_PUBLIC)
  /* Iterator */
  PHP_ME(UserTypeValue, current, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, key, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, next, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, valid, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(UserTypeValue, rewind, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_user_type_value_handlers;

static HashTable *
php_driver_user_type_value_gc(zval *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_user_type_value_properties(zval *object)
{
  zval values;

  php_driver_user_type_value *self = PHP_DRIVER_GET_USER_TYPE_VALUE(object);
  HashTable                 *props = zend_std_get_properties(object);

  zend_hash_str_update(props, "type", strlen("type"), &(self->type));
  Z_ADDREF_P(&(self->type));


  array_init(&(values));
  php_driver_user_type_value_populate(self, &(values));
  zend_hash_str_update(props, "values", strlen("values"), &(values));

  return props;
}

static int
php_driver_user_type_value_compare(zval *obj1, zval *obj2)
{
  HashPosition pos1;
  HashPosition pos2;
  zval *current1;
  zval *current2;
  php_driver_user_type_value *user_type_value1;
  php_driver_user_type_value *user_type_value2;
  php_driver_type *type1;
  php_driver_type *type2;
  int result;

  if (Z_OBJCE_P(obj1) != Z_OBJCE_P(obj2))
    return 1; /* different classes */

  user_type_value1 = PHP_DRIVER_GET_USER_TYPE_VALUE(obj1);
  user_type_value2 = PHP_DRIVER_GET_USER_TYPE_VALUE(obj2);

  type1 = PHP_DRIVER_GET_TYPE(&(user_type_value1->type));
  type2 = PHP_DRIVER_GET_TYPE(&(user_type_value2->type));

  result = php_driver_type_compare(type1, type2);
  if (result != 0) return result;

  if (zend_hash_num_elements(&user_type_value1->values) != zend_hash_num_elements(&user_type_value2->values)) {
    return zend_hash_num_elements(&user_type_value1->values) < zend_hash_num_elements(&user_type_value2->values) ? -1 : 1;
  }

  zend_hash_internal_pointer_reset_ex(&user_type_value1->values, &pos1);
  zend_hash_internal_pointer_reset_ex(&user_type_value2->values, &pos2);

  while (CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&user_type_value1->values, current1, &pos1) &&
         CASS_ZEND_HASH_GET_CURRENT_DATA_EX(&user_type_value2->values, current2, &pos2)) {
    result = php_driver_value_compare(current1,
                                         current2);
    if (result != 0) return result;
    zend_hash_move_forward_ex(&user_type_value1->values, &pos1);
    zend_hash_move_forward_ex(&user_type_value2->values, &pos2);
  }

  return 0;
}

static unsigned
php_driver_user_type_value_hash_value(zval *obj)
{
  zval *current;
  unsigned hashv = 0;
  php_driver_user_type_value *self = PHP_DRIVER_GET_USER_TYPE_VALUE(obj);

  if (!self->dirty) return self->hashv;

  ZEND_HASH_FOREACH_VAL(&self->values, current) {
    hashv = php_driver_combine_hash(hashv,
                                       php_driver_value_hash(current));
  } ZEND_HASH_FOREACH_END();

  self->hashv = hashv;
  self->dirty = 0;

  return hashv;
}

static void
php_driver_user_type_value_free(zend_object *object)
{
  php_driver_user_type_value *self =
          php_driver_user_type_value_object_fetch(object);;

  zend_hash_destroy(&self->values);
  CASS_ZVAL_MAYBE_DESTROY(self->type);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_user_type_value_new(zend_class_entry *ce)
{
  php_driver_user_type_value *self =
      CASS_ZEND_OBJECT_ECALLOC(user_type_value, ce);

  zend_hash_init(&self->values, 0, NULL, ZVAL_PTR_DTOR, 0);
  self->pos = HT_INVALID_IDX;
  self->dirty = 1;
  ZVAL_UNDEF(&(self->type));

  CASS_ZEND_OBJECT_INIT(user_type_value, self, ce);
}

void php_driver_define_UserTypeValue()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\UserTypeValue", php_driver_user_type_value_methods);
  php_driver_user_type_value_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_user_type_value_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_user_type_value_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_user_type_value_handlers.std.get_properties  = php_driver_user_type_value_properties;
#if PHP_VERSION_ID >= 50400
  php_driver_user_type_value_handlers.std.get_gc          = php_driver_user_type_value_gc;
#endif
  php_driver_user_type_value_handlers.std.compare_objects = php_driver_user_type_value_compare;
  php_driver_user_type_value_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_user_type_value_ce->create_object = php_driver_user_type_value_new;
  zend_class_implements(php_driver_user_type_value_ce, 2, spl_ce_Countable, zend_ce_iterator);

  php_driver_user_type_value_handlers.hash_value = php_driver_user_type_value_hash_value;
  php_driver_user_type_value_handlers.std.clone_obj = NULL;
}
