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
#include <time.h>
#include <ext/date/php_date.h>

zend_class_entry *php_driver_date_ce = NULL;

void
php_driver_date_init(INTERNAL_FUNCTION_PARAMETERS)
{
  zval *seconds = NULL;
  php_driver_date *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &seconds) == FAILURE) {
    return;
  }

  if (getThis() && instanceof_function(Z_OBJCE_P(getThis()), php_driver_date_ce)) {
    self = PHP_DRIVER_GET_DATE(getThis());
  } else {
    object_init_ex(return_value, php_driver_date_ce);
    self = PHP_DRIVER_GET_DATE(return_value);
  }

  if (seconds == NULL) {
    self->date = cass_date_from_epoch(time(NULL));
  } else {
    if (Z_TYPE_P(seconds) != IS_LONG) {
      INVALID_ARGUMENT(seconds, "a number of seconds since the Unix Epoch");
    }
    self->date = cass_date_from_epoch(Z_LVAL_P(seconds));
  }
}

/* {{{ Date::__construct(string) */
PHP_METHOD(Date, __construct)
{
  php_driver_date_init(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */

/* {{{ Date::type() */
PHP_METHOD(Date, type)
{
  zval type = php_driver_type_scalar(CASS_VALUE_TYPE_DATE);
  RETURN_ZVAL(&(type), 1, 1);
}
/* }}} */

/* {{{ Date::seconds() */
PHP_METHOD(Date, seconds)
{
  php_driver_date *self = PHP_DRIVER_GET_DATE(getThis());

  RETURN_LONG(cass_date_time_to_epoch(self->date, 0));
}
/* }}} */

/* {{{ Date::toDateTime() */
PHP_METHOD(Date, toDateTime)
{
  php_driver_date *self;
  zval *ztime = NULL;
  php_driver_time* time_obj = NULL;
  zval datetime;
  php_date_obj *datetime_obj = NULL;
  char *str;
  int str_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &ztime) == FAILURE) {
    return;
  }

  if (ztime != NULL) {
    time_obj = PHP_DRIVER_GET_TIME(ztime);
  }
  self = PHP_DRIVER_GET_DATE(getThis());


  php_date_instantiate(php_date_get_date_ce(), &(datetime));

  datetime_obj = php_date_obj_from_obj(Z_OBJ(datetime));

  str_len = spprintf(&str, 0, "%" PRId64,
                     cass_date_time_to_epoch(self->date,
                                             time_obj != NULL ? time_obj->time : 0));
  php_date_initialize(datetime_obj, str, str_len, "U", NULL, 0);
  efree(str);

  RETVAL_ZVAL(&(datetime), 0, 1);
}
/* }}} */

/* {{{ Date::fromDateTime() */
PHP_METHOD(Date, fromDateTime)
{
  php_driver_date *self;
  zval *zdatetime;
  zval retval;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zdatetime) == FAILURE) {
    return;
  }

  CASS_COMPAT_zend_call_method_with_0_params(zdatetime,
                                 php_date_get_date_ce(),
                                 NULL,
                                 "gettimestamp",
                                 &retval);

  if (!Z_ISUNDEF(retval) &&
      Z_TYPE_P(&(retval)) == IS_LONG) {
    object_init_ex(return_value, php_driver_date_ce);
    self = PHP_DRIVER_GET_DATE(return_value);
    self->date = cass_date_from_epoch(Z_LVAL(retval));
    zval_ptr_dtor(&retval);
    return;
  }
}
/* }}} */


/* {{{ Date::__toString() */
PHP_METHOD(Date, __toString)
{
  php_driver_date *self;
  char *ret = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_DATE(getThis());

  spprintf(&ret, 0, PHP_DRIVER_NAMESPACE "\\Date(seconds=%" PRId64 ")", cass_date_time_to_epoch(self->date, 0));
  RETVAL_STRING(ret);
  efree(ret);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, seconds)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_time, 0, ZEND_RETURN_VALUE, 0)
  PHP_DRIVER_NAMESPACE_ZEND_ARG_OBJ_INFO(0, time, Time, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_datetime, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_OBJ_INFO(0, datetime, DateTime, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_date_methods[] = {
  PHP_ME(Date, __construct, arginfo__construct, ZEND_ACC_CTOR|ZEND_ACC_PUBLIC)
  PHP_ME(Date, type, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Date, seconds, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(Date, toDateTime, arginfo_time, ZEND_ACC_PUBLIC)
  PHP_ME(Date, fromDateTime, arginfo_datetime, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
  PHP_ME(Date, __toString, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static php_driver_value_handlers php_driver_date_handlers;

static HashTable *
php_driver_date_gc(zval *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable *
php_driver_date_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval type;
  zval seconds;

  php_driver_date *self = CASS_COMPAT_GET_DATE(object);
  HashTable *props = zend_std_get_properties(object);

  type = php_driver_type_scalar(CASS_VALUE_TYPE_DATE);
  zend_hash_str_update(props, "type", strlen("type"), &(type));


  ZVAL_LONG(&(seconds), cass_date_time_to_epoch(self->date, 0));
  zend_hash_str_update(props, "seconds", strlen("seconds"), &(seconds));

  return props;
}

static int
php_driver_date_compare(zval *obj1, zval *obj2)
{
  php_driver_date *date1 = NULL;
  php_driver_date *date2 = NULL;
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  date1 = PHP_DRIVER_GET_DATE(obj1);
  date2 = PHP_DRIVER_GET_DATE(obj2);

  return PHP_DRIVER_COMPARE(date1->date, date2->date);
}

static unsigned
php_driver_date_hash_value(zval *obj)
{
  php_driver_date *self = PHP_DRIVER_GET_DATE(obj);
  return 31 * 17 + self->date;
}

static void
php_driver_date_free(zend_object *object)
{
  php_driver_date *self = php_driver_date_object_fetch(object);;

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_date_new(zend_class_entry *ce)
{
  php_driver_date *self =
      CASS_ZEND_OBJECT_ECALLOC(date, ce);

  self->date = 0;

  CASS_ZEND_OBJECT_INIT(date, self, ce);
}

void php_driver_define_Date()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Date", php_driver_date_methods);
  php_driver_date_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_date_ce, 1, php_driver_value_ce);
  memcpy(&php_driver_date_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_date_handlers.std.get_properties  = php_driver_date_properties;
  php_driver_date_handlers.std.get_gc          = php_driver_date_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_date_handlers.std, php_driver_date_compare);
  php_driver_date_ce->ce_flags |= ZEND_ACC_FINAL;
  php_driver_date_ce->create_object = php_driver_date_new;

  php_driver_date_handlers.hash_value = php_driver_date_hash_value;
}
