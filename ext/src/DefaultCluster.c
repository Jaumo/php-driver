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

zend_class_entry *php_driver_default_cluster_ce = NULL;

static void
free_session(void *session)
{
  cass_session_free((CassSession*) session);
}

PHP_METHOD(DefaultCluster, connect)
{
  char *keyspace = NULL;
  size_t keyspace_len;
  zval *timeout = NULL;
  php_driver_cluster *self = NULL;
  php_driver_session *session = NULL;
  CassFuture *future = NULL;
  char *hash_key;
  size_t hash_key_len = 0;
  php_driver_psession *psession;


  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sz", &keyspace, &keyspace_len, &timeout) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER(getThis());

  object_init_ex(return_value, php_driver_default_session_ce);
  session = PHP_DRIVER_GET_SESSION(return_value);

  session->default_consistency = self->default_consistency;
  session->default_page_size   = self->default_page_size;
  session->persist             = self->persist;

  if (!Z_ISUNDEF(session->default_timeout)) {
    ZVAL_COPY(&(session->default_timeout),
                      &(self->default_timeout));
  }

  if (session->persist) {
    zval *le;

    hash_key_len = spprintf(&hash_key, 0, "%s:session:%s",
                            self->hash_key, SAFE_STR(keyspace));

    if (CASS_ZEND_HASH_FIND(&EG(persistent_list), hash_key, hash_key_len + 1, le) &&
        Z_RES_P(le)->type == php_le_php_driver_session()) {
      psession = (php_driver_psession *) Z_RES_P(le)->ptr;
      session->session = php_driver_add_ref(psession->session);
      future = psession->future;
    }
  }

  if (future == NULL) {
    zval resource;

    session->session = php_driver_new_peref(cass_session_new(), free_session, 1);

    if (keyspace) {
      future = cass_session_connect_keyspace((CassSession *) session->session->data,
                                             self->cluster,
                                             keyspace);
    } else {
      future = cass_session_connect((CassSession *) session->session->data,
                                    self->cluster);
    }

    if (session->persist) {
      psession = (php_driver_psession *) pecalloc(1, sizeof(php_driver_psession), 1);
      psession->session = php_driver_add_ref(session->session);
      psession->future  = future;

      ZVAL_NEW_PERSISTENT_RES(&resource, 0, psession, php_le_php_driver_session());
      zend_hash_str_update(&EG(persistent_list), hash_key, hash_key_len, &resource);
      PHP_DRIVER_G(persistent_sessions)++;
    }
  }

  if (php_driver_future_wait_timed(future, timeout) == FAILURE) {
    if (session->persist) {
      efree(hash_key);
    } else {
      cass_future_free(future);
    }

    return;
  }

  if (php_driver_future_is_error(future) == FAILURE) {
    if (session->persist) {
      zend_hash_str_del(&EG(persistent_list), hash_key, strlen(hash_key));
      efree(hash_key);
    } else {
      cass_future_free(future);
    }

    return;
  }

  if (session->persist)
    efree(hash_key);
}

PHP_METHOD(DefaultCluster, connectAsync)
{
  char *hash_key = NULL;
  size_t hash_key_len = 0;
  char *keyspace = NULL;
  size_t keyspace_len;
  php_driver_cluster *self = NULL;
  php_driver_future_session *future = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &keyspace, &keyspace_len) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER(getThis());

  object_init_ex(return_value, php_driver_future_session_ce);
  future = PHP_DRIVER_GET_FUTURE_SESSION(return_value);

  future->persist = self->persist;

  if (self->persist) {
    zval *le;

    hash_key_len = spprintf(&hash_key, 0, "%s:session:%s",
                            self->hash_key, SAFE_STR(keyspace));

    future->hash_key     = hash_key;
    future->hash_key_len = hash_key_len;

    if (CASS_ZEND_HASH_FIND(&EG(persistent_list), hash_key, hash_key_len + 1, le) &&
        Z_RES_P(le)->type == php_le_php_driver_session()) {
      php_driver_psession *psession = (php_driver_psession *) Z_RES_P(le)->ptr;
      future->session = php_driver_add_ref(psession->session);
      future->future  = psession->future;
      return;
    }
  }

  future->session = php_driver_new_peref(cass_session_new(), free_session, 1);

  if (keyspace) {
    future->future = cass_session_connect_keyspace((CassSession *) future->session->data,
                                                   self->cluster,
                                                   keyspace);
  } else {
    future->future = cass_session_connect((CassSession *) future->session->data,
                                          self->cluster);
  }

  if (self->persist) {
    zval resource;
    php_driver_psession *psession =
      (php_driver_psession *) pecalloc(1, sizeof(php_driver_psession), 1);
    psession->session = php_driver_add_ref(future->session);
    psession->future  = future->future;

    ZVAL_NEW_PERSISTENT_RES(&resource, 0, psession, php_le_php_driver_session());
    zend_hash_str_update(&EG(persistent_list), hash_key, hash_key_len, &resource);
    PHP_DRIVER_G(persistent_sessions)++;

  }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_connect, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, keyspace)
  ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_connectAsync, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, keyspace)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_default_cluster_methods[] = {
  PHP_ME(DefaultCluster, connect, arginfo_connect, ZEND_ACC_PUBLIC)
  PHP_ME(DefaultCluster, connectAsync, arginfo_connectAsync, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_default_cluster_handlers;

static int
php_driver_default_cluster_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_default_cluster_free(zend_object *object)
{
  php_driver_cluster *self = php_driver_cluster_object_fetch(object);;

  if (self->persist) {
    efree(self->hash_key);
  } else {
    if (self->cluster) {
      cass_cluster_free(self->cluster);
    }
  }

  CASS_ZVAL_MAYBE_DESTROY(self->default_timeout);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_default_cluster_new(zend_class_entry *ce)
{
  php_driver_cluster *self =
      CASS_ZEND_OBJECT_ECALLOC(cluster, ce);

  self->cluster             = NULL;
  self->default_consistency = PHP_DRIVER_DEFAULT_CONSISTENCY;
  self->default_page_size   = 5000;
  self->persist             = 0;
  self->hash_key            = NULL;

  ZVAL_UNDEF(&(self->default_timeout));

  CASS_ZEND_OBJECT_INIT_EX(cluster, default_cluster, self, ce);
}

void php_driver_define_DefaultCluster()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\DefaultCluster", php_driver_default_cluster_methods);
  php_driver_default_cluster_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_default_cluster_ce, 1, php_driver_cluster_ce);
  php_driver_default_cluster_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_default_cluster_ce->create_object = php_driver_default_cluster_new;

  memcpy(&php_driver_default_cluster_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_default_cluster_handlers, php_driver_default_cluster_compare);
}
