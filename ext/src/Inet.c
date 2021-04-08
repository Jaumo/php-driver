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
#include "util/inet.h"
#include "util/types.h"

zend_class_entry *php_driver_inet_ce = NULL;

void
php_driver_inet_init(INTERNAL_FUNCTION_PARAMETERS)
{
  php_driver_inet *self;
  char *string;
  size_t string_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &string, &string_len) == FAILURE) {
    return;
  }

  if (getThis() && instanceof_function(Z_OBJCE_P(getThis()), php_driver_inet_ce)) {
    self = PHP_DRIVER_GET_INET(getThis());
  } else {
    object_init_ex(return_value, php_driver_inet_ce);
    self = PHP_DRIVER_GET_INET(return_value);
  }

  if (!php_driver_parse_ip_address(string, &self->inet)) {
    return;
  }
}

/* {{{ Inet::__construct(string) */
PHP_METHOD(Inet, __construct)
{
  php_driver_inet_init(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Inet::__toString() */
PHP_METHOD(Inet, __toString)
{
  php_driver_inet *inet = PHP_DRIVER_GET_INET(getThis());
  char *string;
  php_driver_format_address(inet->inet, &string);

  RETVAL_STRING(string);
  efree(string);
}
/* }}} */

/* {{{ Inet::type() */
PHP_METHOD(Inet, type)
{
  zval type = php_driver_type_scalar(CASS_VALUE_TYPE_INET);
  RETURN_ZVAL(&(type), 1, 1);
}
/* }}} */

/* {{{ Inet::address() */
PHP_METHOD(Inet, address)
{
  php_driver_inet *inet = PHP_DRIVER_GET_INET(getThis());
  char *string;
  php_driver_format_address(inet->inet, &string);

  RETVAL_STRING(string);
  efree(string);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, address)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_inet_methods[] = {
  PHP_ME(Inet, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Inet, __toString, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Inet, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Inet, address, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_inet_handlers;

static HashTable *
php_driver_inet_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_inet_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  char *string;
  zval type;
  zval address;

  php_driver_inet *self = CASS_COMPAT_GET_INET(object);
  HashTable      *props = zend_std_get_properties(object);

  type = php_driver_type_scalar(CASS_VALUE_TYPE_INET);
  zend_hash_str_update(props, "type", strlen("type"), &(type));

  php_driver_format_address(self->inet, &string);

  ZVAL_STRING(&(address), string);
  efree(string);
  zend_hash_str_update(props, "address", strlen("address"), &(address));

  return props;
}

static int
php_driver_inet_compare(zval *obj1, zval *obj2)
{
  php_driver_inet *inet1 = NULL;
  php_driver_inet *inet2 = NULL;

  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  inet1 = PHP_DRIVER_GET_INET(obj1);
  inet2 = PHP_DRIVER_GET_INET(obj2);

  if (inet1->inet.address_length != inet2->inet.address_length) {
   return inet1->inet.address_length < inet2->inet.address_length ? -1 : 1;
  }
  return memcmp(inet1->inet.address, inet2->inet.address, inet1->inet.address_length);
}

static unsigned
php_driver_inet_hash_value(zval *obj)
{
  php_driver_inet *self = PHP_DRIVER_GET_INET(obj);
  return zend_inline_hash_func((const char *) self->inet.address,
                               self->inet.address_length);
}

static void
php_driver_inet_free(zend_object *object)
{
  php_driver_inet *self = php_driver_inet_object_fetch(object);;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_inet_new(zend_class_entry *ce)
{
  php_driver_inet *self =
      CASS_ZEND_OBJECT_ECALLOC(inet, ce);

  CASS_ZEND_OBJECT_INIT(inet, self, ce);
}

void php_driver_define_Inet()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Inet", php_driver_inet_methods);
  php_driver_inet_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_inet_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_inet_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_inet_handlers.std.get_properties  = php_driver_inet_properties;
  php_driver_inet_handlers.std.get_gc          = php_driver_inet_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_inet_handlers.std, php_driver_inet_compare);
  php_driver_inet_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_inet_ce->create_object = php_driver_inet_new;

  php_driver_inet_handlers.hash_value = php_driver_inet_hash_value;
  php_driver_inet_handlers.std.clone_obj = NULL;
}
