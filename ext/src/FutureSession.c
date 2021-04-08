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
#include "php_driver_globals.h"
#include "php_driver_types.h"
#include "util/future.h"
#include "util/ref.h"

zend_class_entry *php_driver_future_session_ce = NULL;

PHP_METHOD(FutureSession, get)
{
  zval *timeout = NULL;
  CassError rc = CASS_OK;
  php_driver_session *session = NULL;
  php_driver_future_session *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &timeout) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_FUTURE_SESSION(getThis());

  if (self->exception_message) {
    zend_throw_exception_ex(exception_class(self->exception_code),
                            self->exception_code, "%s", self->exception_message);
    return;
  }

  if (!Z_ISUNDEF(self->default_session)) {
    RETURN_ZVAL(&(self->default_session), 1, 0);
  }

  object_init_ex(return_value, php_driver_default_session_ce);
  session = PHP_DRIVER_GET_SESSION(return_value);

  session->session = php_driver_add_ref(self->session);
  session->persist = self->persist;

  if (php_driver_future_wait_timed(self->future, timeout) == FAILURE) {
    return;
  }

  rc = cass_future_error_code(self->future);

  if (rc != CASS_OK) {
    const char *message;
    size_t message_len;
    cass_future_error_message(self->future, &message, &message_len);

    if (self->persist) {
      self->exception_message = estrndup(message, message_len);
      self->exception_code    = rc;

      if (zend_hash_str_del(&EG(persistent_list), self->hash_key, self->hash_key_len)) {
        self->future  = NULL;
      }

      zend_throw_exception_ex(exception_class(self->exception_code),
                              self->exception_code, "%s", self->exception_message);
      return;
    }

    zend_throw_exception_ex(exception_class(rc), rc,
                            "%.*s", (int) message_len, message);
    return;
  }

  ZVAL_COPY(&(self->default_session), return_value);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_timeout, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_future_session_methods[] = {
  PHP_ME(FutureSession, get, arginfo_timeout, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_future_session_handlers;

static int
php_driver_future_session_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_future_session_free(zend_object *object)
{
  php_driver_future_session *self =
          php_driver_future_session_object_fetch(object);;

  if (self->persist) {
    efree(self->hash_key);
  } else {
    if (self->future) {
      cass_future_free(self->future);
    }
  }

  php_driver_del_peref(&self->session, 1);

  if (self->exception_message) {
    efree(self->exception_message);
  }

  CASS_ZVAL_MAYBE_DESTROY(self->default_session);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_future_session_new(zend_class_entry *ce)
{
  php_driver_future_session *self
      = CASS_ZEND_OBJECT_ECALLOC(future_session, ce);

  self->session           = NULL;
  self->future            = NULL;
  self->exception_message = NULL;
  self->hash_key          = NULL;
  self->persist           = 0;

  ZVAL_UNDEF(&(self->default_session));

  CASS_ZEND_OBJECT_INIT(future_session, self, ce);
}

void php_driver_define_FutureSession()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\FutureSession", php_driver_future_session_methods);
  php_driver_future_session_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_future_session_ce, 1, php_driver_future_ce);
  php_driver_future_session_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_future_session_ce->create_object = php_driver_future_session_new;

  memcpy(&php_driver_future_session_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_future_session_handlers, php_driver_future_session_compare);
  php_driver_future_session_handlers.clone_obj = NULL;
}
