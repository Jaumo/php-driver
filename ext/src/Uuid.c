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
#include "util/hash.h"
#include "util/types.h"
#include "util/uuid_gen.h"

zend_class_entry *php_driver_uuid_ce = NULL;

void
php_driver_uuid_init(INTERNAL_FUNCTION_PARAMETERS)
{
  char *value;
  size_t value_len;
  php_driver_uuid *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &value, &value_len) == FAILURE) {
    return;
  }

  if (getThis() && instanceof_function(Z_OBJCE_P(getThis()), php_driver_uuid_ce)) {
    self = PHP_DRIVER_GET_UUID(getThis());
  } else {
    object_init_ex(return_value, php_driver_uuid_ce);
    self = PHP_DRIVER_GET_UUID(return_value);
  }

  if (ZEND_NUM_ARGS() == 0) {
    php_driver_uuid_generate_random(&self->uuid);
  } else {
    if (cass_uuid_from_string(value, &self->uuid) != CASS_OK) {
      zend_throw_exception_ex(php_driver_invalid_argument_exception_ce, 0,
                              "Invalid UUID: '%.*s'", value_len, value);
      return;
    }
  }
}

/* {{{ Uuid::__construct(string) */
PHP_METHOD(Uuid, __construct)
{
  php_driver_uuid_init(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Uuid::__toString() */
PHP_METHOD(Uuid, __toString)
{
  char string[CASS_UUID_STRING_LENGTH];
  php_driver_uuid *self = PHP_DRIVER_GET_UUID(getThis());

  cass_uuid_string(self->uuid, string);

  RETVAL_STRING(string);
}
/* }}} */

/* {{{ Uuid::type() */
PHP_METHOD(Uuid, type)
{
  zval type = php_driver_type_scalar(CASS_VALUE_TYPE_UUID);
  RETURN_ZVAL(&(type), 1, 1);
}
/* }}} */

/* {{{ Uuid::value() */
PHP_METHOD(Uuid, uuid)
{
  char string[CASS_UUID_STRING_LENGTH];
  php_driver_uuid *self = PHP_DRIVER_GET_UUID(getThis());

  cass_uuid_string(self->uuid, string);

  RETVAL_STRING(string);
}
/* }}} */

/* {{{ Uuid::version() */
PHP_METHOD(Uuid, version)
{
  php_driver_uuid *self = PHP_DRIVER_GET_UUID(getThis());

  RETURN_LONG((long) cass_uuid_version(self->uuid));
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, uuid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_uuid_methods[] = {
  PHP_ME(Uuid, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Uuid, __toString, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Uuid, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Uuid, uuid, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Uuid, version, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_uuid_handlers;

static HashTable *
php_driver_uuid_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_uuid_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  char string[CASS_UUID_STRING_LENGTH];
  zval type;
  zval uuid;
  zval version;

  php_driver_uuid *self = CASS_COMPAT_GET_UUID(object);
  HashTable      *props = zend_std_get_properties(object);

  cass_uuid_string(self->uuid, string);

  type = php_driver_type_scalar(CASS_VALUE_TYPE_UUID);
  zend_hash_str_update(props, "type", strlen("type"), &(type));


  ZVAL_STRING(&(uuid), string);
  zend_hash_str_update(props, "uuid", strlen("uuid"), &(uuid));


  ZVAL_LONG(&(version), (long) cass_uuid_version(self->uuid));
  zend_hash_str_update(props, "version", strlen("version"), &(version));

  return props;
}

static int
php_driver_uuid_compare(zval *obj1, zval *obj2)
{
  php_driver_uuid *uuid1 = NULL;
  php_driver_uuid *uuid2 = NULL;

  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  uuid1 = PHP_DRIVER_GET_UUID(obj1);
  uuid2 = PHP_DRIVER_GET_UUID(obj2);

  if (uuid1->uuid.time_and_version != uuid2->uuid.time_and_version)
    return uuid1->uuid.time_and_version < uuid2->uuid.time_and_version ? -1 : 1;
  if (uuid1->uuid.clock_seq_and_node != uuid2->uuid.clock_seq_and_node)
    return uuid1->uuid.clock_seq_and_node < uuid2->uuid.clock_seq_and_node ? -1 : 1;

  return 0;
}

static unsigned
php_driver_uuid_hash_value(zval *obj)
{
  php_driver_uuid *self = PHP_DRIVER_GET_UUID(obj);
  return php_driver_combine_hash(php_driver_bigint_hash(self->uuid.time_and_version),
                                    php_driver_bigint_hash(self->uuid.clock_seq_and_node));

}

static void
php_driver_uuid_free(zend_object *object)
{
  php_driver_uuid *self = php_driver_uuid_object_fetch(object);;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_uuid_new(zend_class_entry *ce)
{
  php_driver_uuid *self =
      CASS_ZEND_OBJECT_ECALLOC(uuid, ce);

  CASS_ZEND_OBJECT_INIT(uuid, self, ce);
}

void
php_driver_define_Uuid()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Uuid", php_driver_uuid_methods);
  php_driver_uuid_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_uuid_ce, 2, php_driver_value_ce, php_driver_uuid_interface_ce);
  memcpy(&php_driver_uuid_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_uuid_handlers.std.get_properties  = php_driver_uuid_properties;
  php_driver_uuid_handlers.std.get_gc          = php_driver_uuid_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_uuid_handlers.std, php_driver_uuid_compare);
  php_driver_uuid_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_uuid_ce->create_object = php_driver_uuid_new;

  php_driver_uuid_handlers.hash_value = php_driver_uuid_hash_value;
  php_driver_uuid_handlers.std.clone_obj = NULL;
}
