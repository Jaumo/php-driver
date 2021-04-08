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
#include "util/types.h"
#include "src/UserTypeValue.h"
#include "util/collections.h"

#include <zend_smart_str.h>

zend_class_entry *php_driver_type_user_type_ce = NULL;

int php_driver_type_user_type_add(php_driver_type *type,
                                     const char *name, size_t name_length,
                                     zval *zsub_type)
{
  php_driver_type *sub_type = PHP_DRIVER_GET_TYPE(zsub_type);
  if (cass_data_type_add_sub_type_by_name_n(type->data_type,
                                            name, name_length,
                                            sub_type->data_type) != CASS_OK) {
    return 0;
  }
  zend_hash_str_add(&type->data.udt.types, name, name_length, zsub_type);
  return 1;
}

PHP_METHOD(TypeUserType, __construct)
{
  zend_throw_exception_ex(php_driver_logic_exception_ce, 0,
    "Instantiation of a " PHP_DRIVER_NAMESPACE "\\Type\\UserType type is not supported."
  );
  return;
}

PHP_METHOD(TypeUserType, withName)
{
  char *name;
  size_t name_len;
  php_driver_type *self;
  php_driver_type *user_type;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  object_init_ex(return_value, php_driver_type_user_type_ce);
  user_type = PHP_DRIVER_GET_TYPE(return_value);
  user_type->data_type = cass_data_type_new_from_existing(self->data_type);

  user_type->data.udt.type_name = estrndup(name, name_len);

  if (self->data.udt.keyspace) {
    user_type->data.udt.keyspace = estrdup(self->data.udt.keyspace);
  }

  CASS_ZEND_HASH_ZVAL_COPY(&user_type->data.udt.types, &self->data.udt.types);
}

PHP_METHOD(TypeUserType, name)
{
  php_driver_type *self;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  if (!self->data.udt.type_name)
    RETURN_NULL();

  RETVAL_STRING(self->data.udt.type_name);
}

PHP_METHOD(TypeUserType, withKeyspace)
{
  char *keyspace;
  size_t keyspace_len;
  php_driver_type *self;
  php_driver_type *user_type;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &keyspace, &keyspace_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  object_init_ex(return_value, php_driver_type_user_type_ce);
  user_type = PHP_DRIVER_GET_TYPE(return_value);
  user_type->data_type = cass_data_type_new_from_existing(self->data_type);

  if (self->data.udt.type_name) {
    user_type->data.udt.type_name = estrdup(self->data.udt.type_name);
  }

  user_type->data.udt.keyspace = estrndup(keyspace, keyspace_len);

  CASS_ZEND_HASH_ZVAL_COPY(&user_type->data.udt.types, &self->data.udt.types);
}

PHP_METHOD(TypeUserType, keyspace)
{
  php_driver_type *self;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  if (!self->data.udt.keyspace)
    RETURN_NULL();

  RETVAL_STRING(self->data.udt.keyspace);
}

PHP_METHOD(TypeUserType, types)
{
  php_driver_type *self;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  array_init(return_value);
  CASS_ZEND_HASH_ZVAL_COPY(Z_ARRVAL_P(return_value), &self->data.udt.types);
}

PHP_METHOD(TypeUserType, __toString)
{
  php_driver_type *self;
  smart_str string = {0};

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  php_driver_type_string(self, &string);
  smart_str_0(&string);

  RETVAL_STRING(CASS_SMART_STR_VAL(string));
  smart_str_free(&string);
}

PHP_METHOD(TypeUserType, create)
{
  php_driver_type *self;
  php_driver_user_type_value *user_type_value;
    zval *args = NULL;
  int argc = 0, i;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "*",
                            &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_TYPE(getThis());

  object_init_ex(return_value, php_driver_user_type_value_ce);
  user_type_value = PHP_DRIVER_GET_USER_TYPE_VALUE(return_value);

  ZVAL_COPY(&(user_type_value->type), getThis());

  if (argc > 0) {
    if (argc % 2 == 1) {
      zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                              "Not enough name/value pairs, user_types can only be created " \
                              "from an even number of name/value pairs, where each odd " \
                              "argument is a name and each even argument is a value, " \
                              "e.g user_type(name, value, name, value, name, value)");

      return;
    }

    for (i = 0; i < argc; i += 2) {
      zval *name = &(args[i]);
      zval *value = &(args[i + 1]);
      zval *sub_type;
      if (Z_TYPE_P(name) != IS_STRING) {
        zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                                "Argument %d is not a string", i + 1);

        return;
      }
      if (!CASS_ZEND_HASH_FIND(&self->data.udt.types,
                               Z_STRVAL_P(name), Z_STRLEN_P(name) + 1,
                               sub_type)) {
        zend_throw_exception_ex(php_driver_invalid_argument_exception_ce,
                                0,
                                "Invalid name '%s'", Z_STRVAL_P(name));

        return;
      }
      if (!php_driver_validate_object(value,
                                      sub_type)) {

        return;
      }
      php_driver_user_type_value_set(user_type_value,
                                     Z_STRVAL_P(name), Z_STRLEN_P(name),
                                     value);
    }


  }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_value, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_name, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_keyspace, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, keyspace)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_type_user_type_methods[] = {
  PHP_ME(TypeUserType, __construct,  arginfo_none,     ZEND_ACC_PRIVATE)
  PHP_ME(TypeUserType, withName,     arginfo_name,     ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, name,         arginfo_none,     ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, withKeyspace, arginfo_keyspace, ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, keyspace,     arginfo_none,     ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, __toString,   arginfo_none,     ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, types,        arginfo_none,     ZEND_ACC_PUBLIC)
  PHP_ME(TypeUserType, create,       arginfo_value,    ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_type_user_type_handlers;

static HashTable *
php_driver_type_user_type_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_type_user_type_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval types;

  php_driver_type *self  = CASS_COMPAT_GET_TYPE(object);
  HashTable      *props = zend_std_get_properties(object);


  array_init(&(types));
  CASS_ZEND_HASH_ZVAL_COPY(Z_ARRVAL(types), &self->data.udt.types);
  zend_hash_str_update(props, "types", strlen("types"), &(types));

  return props;
}

static int
php_driver_type_user_type_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);
  php_driver_type* type1 = PHP_DRIVER_GET_TYPE(obj1);
  php_driver_type* type2 = PHP_DRIVER_GET_TYPE(obj2);

  return php_driver_type_compare(type1, type2);
}

static void
php_driver_type_user_type_free(zend_object *object)
{
  php_driver_type *self = php_driver_type_object_fetch(object);;

  if (self->data_type) cass_data_type_free(self->data_type);
  if (self->data.udt.keyspace) efree(self->data.udt.keyspace);
  if (self->data.udt.type_name) efree(self->data.udt.type_name);
  zend_hash_destroy(&self->data.udt.types);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_type_user_type_new(zend_class_entry *ce)
{
  php_driver_type *self = CASS_ZEND_OBJECT_ECALLOC(type, ce);

  self->type = CASS_VALUE_TYPE_UDT;
  self->data_type = NULL;
  self->data.udt.keyspace = self->data.udt.type_name = NULL;
  zend_hash_init(&self->data.udt.types, 0, NULL, ZVAL_PTR_DTOR, 0);

  CASS_ZEND_OBJECT_INIT_EX(type, type_user_type, self, ce);
}

void php_driver_define_TypeUserType()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Type\\UserType", php_driver_type_user_type_methods);
  php_driver_type_user_type_ce = zend_register_internal_class_ex(&ce, php_driver_type_ce);
  memcpy(&php_driver_type_user_type_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_type_user_type_handlers.get_properties  = php_driver_type_user_type_properties;
  php_driver_type_user_type_handlers.get_gc          = php_driver_type_user_type_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_type_user_type_handlers, php_driver_type_user_type_compare);
  php_driver_type_user_type_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_type_user_type_ce->create_object = php_driver_type_user_type_new;
}
