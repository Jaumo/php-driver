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
#include "util/consistency.h"

#include <zend_smart_str.h>

zend_class_entry *php_driver_cluster_builder_ce = NULL;

PHP_METHOD(ClusterBuilder, build)
{
  CassError rc;
  php_driver_cluster* cluster;
  php_driver_cluster_builder *self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  object_init_ex(return_value, php_driver_default_cluster_ce);
  cluster = PHP_DRIVER_GET_CLUSTER(return_value);

  cluster->persist             = self->persist;
  cluster->default_consistency = self->default_consistency;
  cluster->default_page_size   = self->default_page_size;

  ZVAL_COPY(&(cluster->default_timeout),
                    &(self->default_timeout));

  if (self->persist) {
    cluster->hash_key_len = spprintf(&cluster->hash_key, 0,
                                     PHP_DRIVER_NAME ":%s:%d:%d:%s:%d:%d:%d:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%s:%s:%s:%s",
                                     self->contact_points, self->port, self->load_balancing_policy,
                                     SAFE_STR(self->local_dc), self->used_hosts_per_remote_dc,
                                     self->allow_remote_dcs_for_local_cl, self->use_token_aware_routing,
                                     SAFE_STR(self->username), SAFE_STR(self->password),
                                     self->connect_timeout, self->request_timeout,
                                     self->protocol_version, self->io_threads,
                                     self->core_connections_per_host, self->max_connections_per_host,
                                     self->reconnect_interval, self->enable_latency_aware_routing,
                                     self->enable_tcp_nodelay, self->enable_tcp_keepalive,
                                     self->tcp_keepalive_delay, self->enable_schema,
                                     self->enable_hostname_resolution, self->enable_randomized_contact_points,
                                     self->connection_heartbeat_interval,
                                     SAFE_STR(self->whitelist_hosts), SAFE_STR(self->whitelist_dcs),
                                     SAFE_STR(self->blacklist_hosts), SAFE_STR(self->blacklist_dcs));

    if (self->persist) {
      zval *le;

      if (CASS_ZEND_HASH_FIND(&EG(persistent_list), cluster->hash_key, cluster->hash_key_len + 1, le)) {
        if (Z_TYPE_P(le) == php_le_php_driver_cluster()) {
          cluster->cluster =  (CassCluster*) Z_RES_P(le)->ptr;
          return; /* Return cached version */
        }
      }
    }
  }

  cluster->cluster = cass_cluster_new();

  if (self->load_balancing_policy == LOAD_BALANCING_ROUND_ROBIN) {
    cass_cluster_set_load_balance_round_robin(cluster->cluster);
  }

  if (self->load_balancing_policy == LOAD_BALANCING_DC_AWARE_ROUND_ROBIN) {
    ASSERT_SUCCESS(cass_cluster_set_load_balance_dc_aware(cluster->cluster, self->local_dc,
                                                          self->used_hosts_per_remote_dc, self->allow_remote_dcs_for_local_cl));
  }

  if (self->blacklist_hosts != NULL) {
    cass_cluster_set_blacklist_filtering(cluster->cluster, self->blacklist_hosts);
  }

  if (self->whitelist_hosts != NULL) {
    cass_cluster_set_whitelist_filtering(cluster->cluster, self->whitelist_hosts);
  }

  if (self->blacklist_dcs != NULL) {
    cass_cluster_set_blacklist_dc_filtering(cluster->cluster, self->blacklist_dcs);
  }

  if (self->whitelist_dcs != NULL) {
    cass_cluster_set_whitelist_dc_filtering(cluster->cluster, self->whitelist_dcs);
  }

  cass_cluster_set_token_aware_routing(cluster->cluster, self->use_token_aware_routing);

  if (self->username) {
    cass_cluster_set_credentials(cluster->cluster, self->username, self->password);
  }

  cass_cluster_set_connect_timeout(cluster->cluster, self->connect_timeout);
  cass_cluster_set_request_timeout(cluster->cluster, self->request_timeout);

  if (!Z_ISUNDEF(self->ssl_options)) {
    php_driver_ssl *options = PHP_DRIVER_GET_SSL(&(self->ssl_options));
    cass_cluster_set_ssl(cluster->cluster, options->ssl);
  }

  ASSERT_SUCCESS(cass_cluster_set_contact_points(cluster->cluster, self->contact_points));
  ASSERT_SUCCESS(cass_cluster_set_port(cluster->cluster, self->port));

  ASSERT_SUCCESS(cass_cluster_set_protocol_version(cluster->cluster, self->protocol_version));
  ASSERT_SUCCESS(cass_cluster_set_num_threads_io(cluster->cluster, self->io_threads));
  ASSERT_SUCCESS(cass_cluster_set_core_connections_per_host(cluster->cluster, self->core_connections_per_host));
  ASSERT_SUCCESS(cass_cluster_set_max_connections_per_host(cluster->cluster, self->max_connections_per_host));
  cass_cluster_set_reconnect_wait_time(cluster->cluster, self->reconnect_interval);
  cass_cluster_set_latency_aware_routing(cluster->cluster, self->enable_latency_aware_routing);
  cass_cluster_set_tcp_nodelay(cluster->cluster, self->enable_tcp_nodelay);
  cass_cluster_set_tcp_keepalive(cluster->cluster, self->enable_tcp_keepalive, self->tcp_keepalive_delay);
  cass_cluster_set_use_schema(cluster->cluster, self->enable_schema);

  rc = cass_cluster_set_use_hostname_resolution(cluster->cluster, self->enable_hostname_resolution);
  if (rc == CASS_ERROR_LIB_NOT_IMPLEMENTED) {
    if (self->enable_hostname_resolution) {
      php_error_docref(NULL, E_WARNING,
                        "The underlying C/C++ driver does not implement hostname resolution it will be disabled");
    }
  } else {
    ASSERT_SUCCESS(rc);
  }
  ASSERT_SUCCESS(cass_cluster_set_use_randomized_contact_points(cluster->cluster, self->enable_randomized_contact_points));
  cass_cluster_set_connection_heartbeat_interval(cluster->cluster, self->connection_heartbeat_interval);

  if (!Z_ISUNDEF(self->timestamp_gen)) {
    php_driver_timestamp_gen *timestamp_gen =
        PHP_DRIVER_GET_TIMESTAMP_GEN(&(self->timestamp_gen));
    cass_cluster_set_timestamp_gen(cluster->cluster, timestamp_gen->gen);
  }

  if (!Z_ISUNDEF(self->retry_policy)) {
    php_driver_retry_policy *retry_policy =
        PHP_DRIVER_GET_RETRY_POLICY(&(self->retry_policy));
    cass_cluster_set_retry_policy(cluster->cluster, retry_policy->policy);
  }

  if (self->persist) {
    zval resource;

    ZVAL_NEW_PERSISTENT_RES(&resource, 0, cluster->cluster, php_le_php_driver_cluster());

    zend_hash_str_update(&EG(persistent_list), cluster->hash_key, cluster->hash_key_len, &resource);
    PHP_DRIVER_G(persistent_clusters)++;
  }
}

PHP_METHOD(ClusterBuilder, withDefaultConsistency)
{
  zval *consistency = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &consistency) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (php_driver_get_consistency(consistency, &self->default_consistency) == FAILURE) {
    return;
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withDefaultPageSize)
{
  zval *pageSize = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &pageSize) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(pageSize) == IS_NULL) {
    self->default_page_size = -1;
  } else if (Z_TYPE_P(pageSize) == IS_LONG &&
             Z_LVAL_P(pageSize) > 0) {
    self->default_page_size = Z_LVAL_P(pageSize);
  } else {
    INVALID_ARGUMENT(pageSize, "a positive integer or null");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withDefaultTimeout)
{
  zval *timeout = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &timeout) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(timeout) == IS_NULL) {
    CASS_ZVAL_MAYBE_DESTROY(self->default_timeout);
    ZVAL_UNDEF(&(self->default_timeout));
  } else if ((Z_TYPE_P(timeout) == IS_LONG && Z_LVAL_P(timeout) > 0) ||
             (Z_TYPE_P(timeout) == IS_DOUBLE && Z_LVAL_P(timeout) > 0)) {
    CASS_ZVAL_MAYBE_DESTROY(self->default_timeout);
    ZVAL_COPY(&(self->default_timeout), timeout);
  } else {
    INVALID_ARGUMENT(timeout, "a number of seconds greater than zero or null");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withContactPoints)
{
  zval *host = NULL;
  zval *args = NULL;
  int argc = 0, i;
  smart_str contactPoints = {0};
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  for (i = 0; i < argc; i++) {
    host = &(args[i]);

    if (Z_TYPE_P(host) != IS_STRING) {
      smart_str_free(&contactPoints);
      throw_invalid_argument(host, "host", "a string ip address or hostname");

      return;
    }

    if (i > 0) {
      smart_str_appendl(&contactPoints, ",", 1);
    }

    smart_str_appendl(&contactPoints, Z_STRVAL_P(host), Z_STRLEN_P(host));
  }


  smart_str_0(&contactPoints);

  efree(self->contact_points);
  self->contact_points = estrndup(contactPoints.s->val, contactPoints.s->len);
  smart_str_free(&contactPoints);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withPort)
{
  zval *port = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &port) == FAILURE) {
    return;
  }

 self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(port) == IS_LONG &&
      Z_LVAL_P(port) > 0 &&
      Z_LVAL_P(port) < 65536) {
    self->port = Z_LVAL_P(port);
  } else {
    INVALID_ARGUMENT(port, "an integer between 1 and 65535");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withRoundRobinLoadBalancingPolicy)
{
  php_driver_cluster_builder *self;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (self->local_dc) {
    efree(self->local_dc);
    self->local_dc = NULL;
  }

  self->load_balancing_policy = LOAD_BALANCING_ROUND_ROBIN;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withDatacenterAwareRoundRobinLoadBalancingPolicy)
{
  char *local_dc;
  size_t local_dc_len;
  zval *hostPerRemoteDatacenter = NULL;
  zend_bool allow_remote_dcs_for_local_cl;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "szb", &local_dc, &local_dc_len, &hostPerRemoteDatacenter, &allow_remote_dcs_for_local_cl) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(hostPerRemoteDatacenter) != IS_LONG ||
      Z_LVAL_P(hostPerRemoteDatacenter) < 0) {
    INVALID_ARGUMENT(hostPerRemoteDatacenter, "a positive integer");
  }

  if (self->local_dc) {
    efree(self->local_dc);
    self->local_dc = NULL;
  }

  self->load_balancing_policy         = LOAD_BALANCING_DC_AWARE_ROUND_ROBIN;
  self->local_dc                      = estrndup(local_dc, local_dc_len);
  self->used_hosts_per_remote_dc      = Z_LVAL_P(hostPerRemoteDatacenter);
  self->allow_remote_dcs_for_local_cl = allow_remote_dcs_for_local_cl;

  RETURN_ZVAL(getThis(), 1, 0);
}


PHP_METHOD(ClusterBuilder, withBlackListHosts)
{
  zval *hosts = NULL;
  zval *args = NULL;
  int argc = 0, i;
  smart_str blacklist_hosts = {0};
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  for (i = 0; i < argc; i++) {
    hosts = &(args[i]);

    if (Z_TYPE_P(hosts) != IS_STRING) {
      smart_str_free(&blacklist_hosts);
      throw_invalid_argument(hosts, "hosts", "a string ip address or hostname");

      return;
    }

    if (i > 0) {
      smart_str_appendl(&blacklist_hosts, ",", 1);
    }

    smart_str_appendl(&blacklist_hosts, Z_STRVAL_P(hosts), Z_STRLEN_P(hosts));
  }


  smart_str_0(&blacklist_hosts);

  efree(self->blacklist_hosts);
  self->blacklist_hosts = estrndup(blacklist_hosts.s->val, blacklist_hosts.s->len);
  smart_str_free(&blacklist_hosts);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withWhiteListHosts)
{
  zval *hosts = NULL;
  zval *args = NULL;
  int argc = 0, i;
  smart_str whitelist_hosts = {0};
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  for (i = 0; i < argc; i++) {
    hosts = &(args[i]);

    if (Z_TYPE_P(hosts) != IS_STRING) {
      smart_str_free(&whitelist_hosts);
      throw_invalid_argument(hosts, "hosts", "a string ip address or hostname");

      return;
    }

    if (i > 0) {
      smart_str_appendl(&whitelist_hosts, ",", 1);
    }

    smart_str_appendl(&whitelist_hosts, Z_STRVAL_P(hosts), Z_STRLEN_P(hosts));
  }


  smart_str_0(&whitelist_hosts);

  efree(self->whitelist_hosts);
  self->whitelist_hosts = estrndup(whitelist_hosts.s->val, whitelist_hosts.s->len);
  smart_str_free(&whitelist_hosts);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withBlackListDCs)
{
  zval *dcs = NULL;
  zval *args = NULL;
  int argc = 0, i;
  smart_str blacklist_dcs = {0};
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  for (i = 0; i < argc; i++) {
    dcs = &(args[i]);

    if (Z_TYPE_P(dcs) != IS_STRING) {
      smart_str_free(&blacklist_dcs);
      throw_invalid_argument(dcs, "dcs", "a string");

      return;
    }

    if (i > 0) {
      smart_str_appendl(&blacklist_dcs, ",", 1);
    }

    smart_str_appendl(&blacklist_dcs, Z_STRVAL_P(dcs), Z_STRLEN_P(dcs));
  }


  smart_str_0(&blacklist_dcs);

  efree(self->blacklist_dcs);
  self->blacklist_dcs = estrndup(blacklist_dcs.s->val, blacklist_dcs.s->len);
  smart_str_free(&blacklist_dcs);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withWhiteListDCs)
{
  zval *dcs = NULL;
  zval *args = NULL;
  int argc = 0, i;
  smart_str whitelist_dcs = {0};
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "+", &args, &argc) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  for (i = 0; i < argc; i++) {
    dcs = &(args[i]);

    if (Z_TYPE_P(dcs) != IS_STRING) {
      smart_str_free(&whitelist_dcs);
      throw_invalid_argument(dcs, "dcs", "a string");

      return;
    }

    if (i > 0) {
      smart_str_appendl(&whitelist_dcs, ",", 1);
    }

    smart_str_appendl(&whitelist_dcs, Z_STRVAL_P(dcs), Z_STRLEN_P(dcs));
  }

  smart_str_0(&whitelist_dcs);

  efree(self->whitelist_dcs);
  self->whitelist_dcs = estrndup(whitelist_dcs.s->val, whitelist_dcs.s->len);
  smart_str_free(&whitelist_dcs);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withTokenAwareRouting)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->use_token_aware_routing = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withCredentials)
{
  zval *username = NULL;
  zval *password = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &username, &password) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(username) != IS_STRING) {
    INVALID_ARGUMENT(username, "a string");
  }

  if (Z_TYPE_P(password) != IS_STRING) {
    INVALID_ARGUMENT(password, "a string");
  }

  if (self->username) {
    efree(self->username);
    efree(self->password);
  }

  self->username = estrndup(Z_STRVAL_P(username), Z_STRLEN_P(username));
  self->password = estrndup(Z_STRVAL_P(password), Z_STRLEN_P(password));

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withConnectTimeout)
{
  zval *timeout = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &timeout) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(timeout) == IS_LONG &&
      Z_LVAL_P(timeout) > 0) {
    self->connect_timeout = Z_LVAL_P(timeout) * 1000;
  } else if (Z_TYPE_P(timeout) == IS_DOUBLE &&
             Z_DVAL_P(timeout) > 0) {
    self->connect_timeout = ceil(Z_DVAL_P(timeout) * 1000);
  } else {
    INVALID_ARGUMENT(timeout, "a positive number");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withRequestTimeout)
{
  double timeout;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "d", &timeout) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->request_timeout = ceil(timeout * 1000);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withSSL)
{
  zval *ssl_options = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &ssl_options, php_driver_ssl_ce) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (!Z_ISUNDEF(self->ssl_options))
    zval_ptr_dtor(&self->ssl_options);

  ZVAL_COPY(&(self->ssl_options), ssl_options);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withPersistentSessions)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->persist = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withProtocolVersion)
{
  zval *version = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &version) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(version) == IS_LONG &&
      Z_LVAL_P(version) >= 1) {
    self->protocol_version = Z_LVAL_P(version);
  } else {
    INVALID_ARGUMENT(version, "must be >= 1");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withIOThreads)
{
  zval *count = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &count) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(count) == IS_LONG &&
      Z_LVAL_P(count) < 129 &&
      Z_LVAL_P(count) > 0) {
    self->io_threads = Z_LVAL_P(count);
  } else {
    INVALID_ARGUMENT(count, "a number between 1 and 128");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withConnectionsPerHost)
{
  zval *core = NULL;
  zval *max = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &core, &max) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(core) == IS_LONG &&
      Z_LVAL_P(core) < 129 &&
      Z_LVAL_P(core) > 0) {
    self->core_connections_per_host = Z_LVAL_P(core);
  } else {
    INVALID_ARGUMENT(core, "a number between 1 and 128");
  }

  if (max == NULL || Z_TYPE_P(max) == IS_NULL) {
    self->max_connections_per_host = Z_LVAL_P(core);
  } else if (Z_TYPE_P(max) == IS_LONG) {
    if (Z_LVAL_P(max) < Z_LVAL_P(core)) {
      INVALID_ARGUMENT(max, "greater than core");
    } else if (Z_LVAL_P(max) > 128) {
      INVALID_ARGUMENT(max, "less than 128");
    } else {
      self->max_connections_per_host = Z_LVAL_P(max);
    }
  } else {
    INVALID_ARGUMENT(max, "a number between 1 and 128");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withReconnectInterval)
{
  zval *interval = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &interval) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(interval) == IS_LONG &&
      Z_LVAL_P(interval) > 0) {
    self->reconnect_interval = Z_LVAL_P(interval) * 1000;
  } else if (Z_TYPE_P(interval) == IS_DOUBLE &&
             Z_DVAL_P(interval) > 0) {
    self->reconnect_interval = ceil(Z_DVAL_P(interval) * 1000);
  } else {
    INVALID_ARGUMENT(interval, "a positive number");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withLatencyAwareRouting)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->enable_latency_aware_routing = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withTCPNodelay)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->enable_tcp_nodelay = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withTCPKeepalive)
{
  zval *delay = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &delay) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(delay) == IS_NULL) {
    self->enable_tcp_keepalive = 0;
    self->tcp_keepalive_delay  = 0;
  } else if (Z_TYPE_P(delay) == IS_LONG &&
             Z_LVAL_P(delay) > 0) {
    self->enable_tcp_keepalive = 1;
    self->tcp_keepalive_delay  = Z_LVAL_P(delay) * 1000;
  } else if (Z_TYPE_P(delay) == IS_DOUBLE &&
             Z_DVAL_P(delay) > 0) {
    self->enable_tcp_keepalive = 1;
    self->tcp_keepalive_delay  = ceil(Z_DVAL_P(delay) * 1000);
  } else {
    INVALID_ARGUMENT(delay, "a positive number or null");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withRetryPolicy)
{
  zval *retry_policy = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O",
                            &retry_policy, php_driver_retry_policy_ce) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (!Z_ISUNDEF(self->retry_policy))
    zval_ptr_dtor(&self->retry_policy);

  ZVAL_COPY(&(self->retry_policy), retry_policy);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withTimestampGenerator)
{
  zval *timestamp_gen = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O",
                            &timestamp_gen, php_driver_timestamp_gen_ce) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (!Z_ISUNDEF(self->timestamp_gen))
    zval_ptr_dtor(&self->timestamp_gen);

  ZVAL_COPY(&(self->timestamp_gen), timestamp_gen);

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withSchemaMetadata)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->enable_schema = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withHostnameResolution)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->enable_hostname_resolution = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withRandomizedContactPoints)
{
  zend_bool enabled = 1;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &enabled) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  self->enable_randomized_contact_points = enabled;

  RETURN_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(ClusterBuilder, withConnectionHeartbeatInterval)
{
  zval *interval = NULL;
  php_driver_cluster_builder *self;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &interval) == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_CLUSTER_BUILDER(getThis());

  if (Z_TYPE_P(interval) == IS_LONG &&
      Z_LVAL_P(interval) >= 0) {
    self->connection_heartbeat_interval = Z_LVAL_P(interval);
  } else if (Z_TYPE_P(interval) == IS_DOUBLE &&
             Z_DVAL_P(interval) >= 0) {
    self->connection_heartbeat_interval = ceil(Z_DVAL_P(interval));
  } else {
    INVALID_ARGUMENT(interval, "a positive number (or 0 to disable)");
  }

  RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_consistency, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, consistency)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_page_size, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, pageSize)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_contact_points, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, host)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_port, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dc_aware, 0, ZEND_RETURN_VALUE, 3)
  ZEND_ARG_INFO(0, localDatacenter)
  ZEND_ARG_INFO(0, hostPerRemoteDatacenter)
  ZEND_ARG_INFO(0, useRemoteDatacenterForLocalConsistencies)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_blacklist_nodes, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, hosts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_whitelist_nodes, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, hosts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_blacklist_dcs, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, dcs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_whitelist_dcs, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, dcs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_enabled, 0, ZEND_RETURN_VALUE, 0)
  ZEND_ARG_INFO(0, enabled)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_credentials, 0, ZEND_RETURN_VALUE, 2)
  ZEND_ARG_INFO(0, username)
  ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_timeout, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ssl, 0, ZEND_RETURN_VALUE, 1)
  PHP_DRIVER_NAMESPACE_ZEND_ARG_OBJ_INFO(0, options, SSLOptions, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_version, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, version)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_count, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_connections, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, core)
  ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_interval, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, interval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_delay, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, delay)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_retry_policy, 0, ZEND_RETURN_VALUE, 1)
  PHP_DRIVER_NAMESPACE_ZEND_ARG_OBJ_INFO(0, policy, RetryPolicy, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_timestamp_gen, 0, ZEND_RETURN_VALUE, 1)
  PHP_DRIVER_NAMESPACE_ZEND_ARG_OBJ_INFO(0, generator, TimestampGenerator, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_cluster_builder_methods[] = {
  PHP_ME(ClusterBuilder, build, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withDefaultConsistency, arginfo_consistency, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withDefaultPageSize, arginfo_page_size, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withDefaultTimeout, arginfo_timeout, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withContactPoints, arginfo_contact_points, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withPort, arginfo_port, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withRoundRobinLoadBalancingPolicy, arginfo_none, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withDatacenterAwareRoundRobinLoadBalancingPolicy, arginfo_dc_aware, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withBlackListHosts, arginfo_blacklist_nodes, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withWhiteListHosts, arginfo_whitelist_nodes, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withBlackListDCs, arginfo_blacklist_dcs, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withWhiteListDCs, arginfo_whitelist_dcs, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withTokenAwareRouting, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withCredentials, arginfo_credentials, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withConnectTimeout, arginfo_timeout, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withRequestTimeout, arginfo_timeout, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withSSL, arginfo_ssl, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withPersistentSessions, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withProtocolVersion, arginfo_version, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withIOThreads, arginfo_count, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withConnectionsPerHost, arginfo_connections, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withReconnectInterval, arginfo_interval, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withLatencyAwareRouting, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withTCPNodelay, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withTCPKeepalive, arginfo_delay, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withRetryPolicy, arginfo_retry_policy, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withTimestampGenerator, arginfo_timestamp_gen, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withSchemaMetadata, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withHostnameResolution, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withRandomizedContactPoints, arginfo_enabled, ZEND_ACC_PUBLIC)
  PHP_ME(ClusterBuilder, withConnectionHeartbeatInterval, arginfo_interval, ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_cluster_builder_handlers;

static HashTable*
php_driver_cluster_builder_gc(CASS_COMPAT_OBJECT_HANDLER_TYPE *object, zval **table, int *n)
{
  *table = NULL;
  *n = 0;
  return zend_std_get_properties(object);
}

static HashTable*
php_driver_cluster_builder_properties(CASS_COMPAT_OBJECT_HANDLER_TYPE *object)
{
  zval contactPoints;
  zval loadBalancingPolicy;
  zval localDatacenter;
  zval hostPerRemoteDatacenter;
  zval useRemoteDatacenterForLocalConsistencies;
  zval useTokenAwareRouting;
  zval username;
  zval password;
  zval connectTimeout;
  zval requestTimeout;
  zval sslOptions;
  zval defaultConsistency;
  zval defaultPageSize;
  zval defaultTimeout;
  zval usePersistentSessions;
  zval protocolVersion;
  zval ioThreads;
  zval coreConnectionPerHost;
  zval maxConnectionsPerHost;
  zval reconnectInterval;
  zval latencyAwareRouting;
  zval tcpNodelay;
  zval tcpKeepalive;
  zval retryPolicy;
  zval blacklistHosts;
  zval whitelistHosts;
  zval blacklistDCs;
  zval whitelistDCs;
  zval timestampGen;
  zval schemaMetadata;
  zval hostnameResolution;
  zval randomizedContactPoints;
  zval connectionHeartbeatInterval;

  php_driver_cluster_builder *self = CASS_COMPAT_GET_CLUSTER_BUILDER(object);
  HashTable *props = zend_std_get_properties(object);


  ZVAL_STRING(&(contactPoints), self->contact_points);


  ZVAL_LONG(&(loadBalancingPolicy), self->load_balancing_policy);




  if (self->load_balancing_policy == LOAD_BALANCING_DC_AWARE_ROUND_ROBIN) {
    ZVAL_STRING(&(localDatacenter), self->local_dc);
    ZVAL_LONG(&(hostPerRemoteDatacenter), self->used_hosts_per_remote_dc);
    ZVAL_BOOL(&(useRemoteDatacenterForLocalConsistencies), self->allow_remote_dcs_for_local_cl);
  } else {
    ZVAL_NULL(&(localDatacenter));
    ZVAL_NULL(&(hostPerRemoteDatacenter));
    ZVAL_NULL(&(useRemoteDatacenterForLocalConsistencies));
  }


  ZVAL_BOOL(&(useTokenAwareRouting), self->use_token_aware_routing);



  if (self->username) {
    ZVAL_STRING(&(username), self->username);
    ZVAL_STRING(&(password), self->password);
  } else {
    ZVAL_NULL(&(username));
    ZVAL_NULL(&(password));
  }


  ZVAL_DOUBLE(&(connectTimeout), (double) self->connect_timeout / 1000);

  ZVAL_DOUBLE(&(requestTimeout), (double) self->request_timeout / 1000);


  if (!Z_ISUNDEF(self->ssl_options)) {
    ZVAL_COPY(&(sslOptions), &(self->ssl_options));
  } else {
    ZVAL_NULL(&(sslOptions));
  }


  ZVAL_LONG(&(defaultConsistency), self->default_consistency);

  ZVAL_LONG(&(defaultPageSize), self->default_page_size);

  if (!Z_ISUNDEF(self->default_timeout)) {
    ZVAL_LONG(&(defaultTimeout), Z_LVAL(self->default_timeout));
  } else {
    ZVAL_NULL(&(defaultTimeout));
  }


  ZVAL_BOOL(&(usePersistentSessions), self->persist);


  ZVAL_LONG(&(protocolVersion), self->protocol_version);


  ZVAL_LONG(&(ioThreads), self->io_threads);


  ZVAL_LONG(&(coreConnectionPerHost), self->core_connections_per_host);


  ZVAL_LONG(&(maxConnectionsPerHost), self->max_connections_per_host);


  ZVAL_DOUBLE(&(reconnectInterval), (double) self->reconnect_interval / 1000);


  ZVAL_BOOL(&(latencyAwareRouting), self->enable_latency_aware_routing);


  ZVAL_BOOL(&(tcpNodelay), self->enable_tcp_nodelay);


  if (self->enable_tcp_keepalive) {
    ZVAL_DOUBLE(&(tcpKeepalive), (double) self->tcp_keepalive_delay / 1000);
  } else {
    ZVAL_NULL(&(tcpKeepalive));
  }


  if (!Z_ISUNDEF(self->retry_policy)) {
    ZVAL_COPY(&(retryPolicy), &(self->retry_policy));
  } else {
    ZVAL_NULL(&(retryPolicy));
  }


  if (self->blacklist_hosts) {
    ZVAL_STRING(&(blacklistHosts), self->blacklist_hosts);
  } else {
    ZVAL_NULL(&(blacklistHosts));
  }


  if (self->whitelist_hosts) {
    ZVAL_STRING(&(whitelistHosts), self->whitelist_hosts);
  } else {
    ZVAL_NULL(&(whitelistHosts));
  }


  if (self->blacklist_dcs) {
    ZVAL_STRING(&(blacklistDCs), self->blacklist_dcs);
  } else {
    ZVAL_NULL(&(blacklistDCs));
  }


  if (self->whitelist_dcs) {
    ZVAL_STRING(&(whitelistDCs), self->whitelist_dcs);
  } else {
    ZVAL_NULL(&(whitelistDCs));
  }


  if (!Z_ISUNDEF(self->timestamp_gen)) {
    ZVAL_COPY(&(timestampGen), &(self->timestamp_gen));
  } else {
    ZVAL_NULL(&(timestampGen));
  }


  ZVAL_BOOL(&(schemaMetadata), self->enable_schema);


  ZVAL_BOOL(&(hostnameResolution), self->enable_hostname_resolution);


  ZVAL_BOOL(&(randomizedContactPoints), self->enable_randomized_contact_points);


  ZVAL_LONG(&(connectionHeartbeatInterval), self->connection_heartbeat_interval);

  zend_hash_str_update(props, "contactPoints", strlen("contactPoints"), &(contactPoints));
  zend_hash_str_update(props, "loadBalancingPolicy", strlen("loadBalancingPolicy"), &(loadBalancingPolicy));
  zend_hash_str_update(props, "localDatacenter", strlen("localDatacenter"), &(localDatacenter));
  zend_hash_str_update(props, "hostPerRemoteDatacenter", strlen("hostPerRemoteDatacenter"), &(hostPerRemoteDatacenter));
  zend_hash_str_update(props, "useRemoteDatacenterForLocalConsistencies", strlen("useRemoteDatacenterForLocalConsistencies"), &(useRemoteDatacenterForLocalConsistencies));
  zend_hash_str_update(props, "useTokenAwareRouting", strlen("useTokenAwareRouting"), &(useTokenAwareRouting));
  zend_hash_str_update(props, "username", strlen("username"), &(username));
  zend_hash_str_update(props, "password", strlen("password"), &(password));
  zend_hash_str_update(props, "connectTimeout", strlen("connectTimeout"), &(connectTimeout));
  zend_hash_str_update(props, "requestTimeout", strlen("requestTimeout"), &(requestTimeout));
  zend_hash_str_update(props, "sslOptions", strlen("sslOptions"), &(sslOptions));
  zend_hash_str_update(props, "defaultConsistency", strlen("defaultConsistency"), &(defaultConsistency));
  zend_hash_str_update(props, "defaultPageSize", strlen("defaultPageSize"), &(defaultPageSize));
  zend_hash_str_update(props, "defaultTimeout", strlen("defaultTimeout"), &(defaultTimeout));
  zend_hash_str_update(props, "usePersistentSessions", strlen("usePersistentSessions"), &(usePersistentSessions));
  zend_hash_str_update(props, "protocolVersion", strlen("protocolVersion"), &(protocolVersion));
  zend_hash_str_update(props, "ioThreads", strlen("ioThreads"), &(ioThreads));
  zend_hash_str_update(props, "coreConnectionPerHost", strlen("coreConnectionPerHost"), &(coreConnectionPerHost));
  zend_hash_str_update(props, "maxConnectionsPerHost", strlen("maxConnectionsPerHost"), &(maxConnectionsPerHost));
  zend_hash_str_update(props, "reconnectInterval", strlen("reconnectInterval"), &(reconnectInterval));
  zend_hash_str_update(props, "latencyAwareRouting", strlen("latencyAwareRouting"), &(latencyAwareRouting));
  zend_hash_str_update(props, "tcpNodelay", strlen("tcpNodelay"), &(tcpNodelay));
  zend_hash_str_update(props, "tcpKeepalive", strlen("tcpKeepalive"), &(tcpKeepalive));
  zend_hash_str_update(props, "retryPolicy", strlen("retryPolicy"), &(retryPolicy));
  zend_hash_str_update(props, "timestampGenerator", strlen("timestampGenerator"), &(timestampGen));
  zend_hash_str_update(props, "schemaMetadata", strlen("schemaMetadata"), &(schemaMetadata));
  zend_hash_str_update(props, "blacklist_hosts", strlen("blacklist_hosts"), &(blacklistHosts));
  zend_hash_str_update(props, "whitelist_hosts", strlen("whitelist_hosts"), &(whitelistHosts));
  zend_hash_str_update(props, "blacklist_dcs", strlen("blacklist_dcs"), &(blacklistDCs));
  zend_hash_str_update(props, "whitelist_dcs", strlen("whitelist_dcs"), &(whitelistDCs));
  zend_hash_str_update(props, "hostnameResolution", strlen("hostnameResolution"), &(hostnameResolution));
  zend_hash_str_update(props, "randomizedContactPoints", strlen("randomizedContactPoints"), &(randomizedContactPoints));
  zend_hash_str_update(props, "connectionHeartbeatInterval", strlen("connectionHeartbeatInterval"), &(connectionHeartbeatInterval));

  return props;
}

static int
php_driver_cluster_builder_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_cluster_builder_free(zend_object *object)
{
  php_driver_cluster_builder *self =
          php_driver_cluster_builder_object_fetch(object);;

  efree(self->contact_points);
  self->contact_points = NULL;

  if (self->local_dc) {
    efree(self->local_dc);
    self->local_dc = NULL;
  }

  if (self->username) {
    efree(self->username);
    self->username = NULL;
  }

  if (self->password) {
    efree(self->password);
    self->password = NULL;
  }

  if (self->whitelist_hosts) {
    efree(self->whitelist_hosts);
    self->whitelist_hosts = NULL;
  }

  if (self->blacklist_hosts) {
    efree(self->blacklist_hosts);
    self->blacklist_hosts = NULL;
  }

  if (self->whitelist_dcs) {
    efree(self->whitelist_dcs);
    self->whitelist_dcs = NULL;
  }

  if (self->blacklist_dcs) {
    efree(self->blacklist_dcs);
    self->whitelist_dcs = NULL;
  }

  CASS_ZVAL_MAYBE_DESTROY(self->ssl_options);
  CASS_ZVAL_MAYBE_DESTROY(self->default_timeout);
  CASS_ZVAL_MAYBE_DESTROY(self->retry_policy);
  CASS_ZVAL_MAYBE_DESTROY(self->timestamp_gen);

  zend_object_std_dtor(&self->zval);
}

static zend_object *
php_driver_cluster_builder_new(zend_class_entry *ce)
{
  php_driver_cluster_builder *self =
      CASS_ZEND_OBJECT_ECALLOC(cluster_builder, ce);

  self->contact_points = estrdup("127.0.0.1");
  self->port = 9042;
  self->load_balancing_policy = LOAD_BALANCING_DEFAULT;
  self->local_dc = NULL;
  self->used_hosts_per_remote_dc = 0;
  self->allow_remote_dcs_for_local_cl = 0;
  self->use_token_aware_routing = 1;
  self->username = NULL;
  self->password = NULL;
  self->connect_timeout = 5000;
  self->request_timeout = 12000;
  self->default_consistency = PHP_DRIVER_DEFAULT_CONSISTENCY;
  self->default_page_size = 5000;
  self->persist = 1;
  self->protocol_version = 4;
  self->io_threads = 1;
  self->core_connections_per_host = 1;
  self->max_connections_per_host = 2;
  self->reconnect_interval = 2000;
  self->enable_latency_aware_routing = 1;
  self->enable_tcp_nodelay = 1;
  self->enable_tcp_keepalive = 0;
  self->tcp_keepalive_delay = 0;
  self->enable_schema = 1;
  self->blacklist_hosts = NULL;
  self->whitelist_hosts = NULL;
  self->blacklist_dcs = NULL;
  self->whitelist_dcs = NULL;
  self->enable_hostname_resolution = 0;
  self->enable_randomized_contact_points = 1;
  self->connection_heartbeat_interval = 30;

  ZVAL_UNDEF(&(self->ssl_options));
  ZVAL_UNDEF(&(self->default_timeout));
  ZVAL_UNDEF(&(self->retry_policy));
  ZVAL_UNDEF(&(self->timestamp_gen));

  CASS_ZEND_OBJECT_INIT(cluster_builder, self, ce);
}

void php_driver_define_ClusterBuilder()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Cluster\\Builder", php_driver_cluster_builder_methods);
  php_driver_cluster_builder_ce = zend_register_internal_class(&ce);
  php_driver_cluster_builder_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_cluster_builder_ce->create_object = php_driver_cluster_builder_new;

  memcpy(&php_driver_cluster_builder_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  php_driver_cluster_builder_handlers.get_properties  = php_driver_cluster_builder_properties;
  php_driver_cluster_builder_handlers.get_gc          = php_driver_cluster_builder_gc;
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_cluster_builder_handlers, php_driver_cluster_builder_compare);
}
