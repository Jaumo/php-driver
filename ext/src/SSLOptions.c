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

zend_class_entry *php_driver_ssl_ce = NULL;

static zend_function_entry php_driver_ssl_methods[] = {
  PHP_FE_END
};

static zend_object_handlers php_driver_ssl_handlers;

static int
php_driver_ssl_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_ssl_free(zend_object *object)
{
  php_driver_ssl *self = php_driver_ssl_object_fetch(object);;

  cass_ssl_free(self->ssl);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_ssl_new(zend_class_entry *ce)
{
  php_driver_ssl *self =
      CASS_ZEND_OBJECT_ECALLOC(ssl, ce);

  self->ssl = cass_ssl_new();

  CASS_ZEND_OBJECT_INIT(ssl, self, ce);
}

void php_driver_define_SSLOptions()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\SSLOptions", php_driver_ssl_methods);
  php_driver_ssl_ce = zend_register_internal_class(&ce);
  php_driver_ssl_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_ssl_ce->create_object = php_driver_ssl_new;

  memcpy(&php_driver_ssl_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_ssl_handlers, php_driver_ssl_compare);
  php_driver_ssl_handlers.clone_obj = NULL;
}
