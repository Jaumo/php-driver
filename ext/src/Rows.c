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
#include "util/future.h"
#include "util/ref.h"
#include "util/result.h"

#include "FutureRows.h"

zend_class_entry *php_driver_rows_ce = NULL;

static void
free_result(void *result)
{
  cass_result_free((CassResult *) result);
}

static void
php_driver_rows_create(php_driver_rows *current, zval *result) {
  php_driver_rows *rows;

  if (Z_ISUNDEF(current->next_rows)) {
    if (php_driver_get_result((const CassResult *) current->next_result->data,
                                 &current->next_rows) == FAILURE) {
      CASS_ZVAL_MAYBE_DESTROY(current->next_rows);
      return;
    }
  }

  object_init_ex(result, php_driver_rows_ce);
  rows = PHP_DRIVER_GET_ROWS(result);

  ZVAL_COPY(&(rows->rows),
                    &(current->next_rows));

  if (cass_result_has_more_pages((const CassResult *) current->next_result->data)) {
    rows->statement = php_driver_add_ref(current->statement);
    rows->session   = php_driver_add_ref(current->session);
    rows->result    = php_driver_add_ref(current->next_result);
  }
}

PHP_METHOD(Rows, __construct)
{
  zend_throw_exception_ex(php_driver_logic_exception_ce, 0,
    "Instantiation of a " PHP_DRIVER_NAMESPACE "\\Rows objects directly is not supported, " \
    "call " PHP_DRIVER_NAMESPACE "\\Session::execute() or " PHP_DRIVER_NAMESPACE "\\FutureRows::get() instead."
  );
  return;
}

PHP_METHOD(Rows, count)
{
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  RETURN_LONG(zend_hash_num_elements(Z_ARRVAL_P(&(self->rows))));
}

PHP_METHOD(Rows, rewind)
{
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  zend_hash_internal_pointer_reset(Z_ARRVAL_P(&(self->rows)));
}

PHP_METHOD(Rows, current)
{
  zval *entry;
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_ROWS(getThis());

  if (CASS_ZEND_HASH_GET_CURRENT_DATA(Z_ARRVAL(self->rows), entry)) {
    RETURN_ZVAL(entry, 1, 0);
  }
}

PHP_METHOD(Rows, key)
{
  zend_ulong num_index;
  zend_string *str_index;
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  if (zend_hash_get_current_key(Z_ARRVAL(self->rows),
                                        &str_index, &num_index) == HASH_KEY_IS_LONG)
    RETURN_LONG(num_index);
}

PHP_METHOD(Rows, next)
{
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_ROWS(getThis());

  zend_hash_move_forward(Z_ARRVAL(self->rows));
}

PHP_METHOD(Rows, valid)
{
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  RETURN_BOOL(zend_hash_has_more_elements(Z_ARRVAL(self->rows)) == SUCCESS);
}

PHP_METHOD(Rows, offsetExists)
{
  zval *offset;
  php_driver_rows *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &offset) == FAILURE)
    return;

  if (Z_TYPE_P(offset) != IS_LONG || Z_LVAL_P(offset) < 0) {
    INVALID_ARGUMENT(offset, "a positive integer");
  }

  self = PHP_DRIVER_GET_ROWS(getThis());

  RETURN_BOOL(zend_hash_index_exists(Z_ARRVAL(self->rows),
                                     (zend_ulong) Z_LVAL_P(offset)));
}

PHP_METHOD(Rows, offsetGet)
{
  zval *offset;
  zval *value;
  php_driver_rows *self = NULL;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &offset) == FAILURE)
    return;

  if (Z_TYPE_P(offset) != IS_LONG || Z_LVAL_P(offset) < 0) {
    INVALID_ARGUMENT(offset, "a positive integer");
  }

  self = PHP_DRIVER_GET_ROWS(getThis());
  if (CASS_ZEND_HASH_INDEX_FIND(Z_ARRVAL(self->rows), Z_LVAL_P(offset), value)) {
    RETURN_ZVAL(value, 1, 0);
  }
}

PHP_METHOD(Rows, offsetSet)
{
  if (zend_parse_parameters_none() == FAILURE)
    return;

  zend_throw_exception_ex(php_driver_domain_exception_ce, 0,
    "Cannot overwrite a row at a given offset, rows are immutable."
  );
  return;
}

PHP_METHOD(Rows, offsetUnset)
{
  if (zend_parse_parameters_none() == FAILURE)
    return;

  zend_throw_exception_ex(php_driver_domain_exception_ce, 0,
    "Cannot delete a row at a given offset, rows are immutable."
  );
  return;
}

PHP_METHOD(Rows, isLastPage)
{
  php_driver_rows *self = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  if (self->result == NULL &&
      Z_ISUNDEF(self->next_rows) &&
      Z_ISUNDEF(self->future_next_page)) {
    RETURN_TRUE;
  }

  RETURN_FALSE;
}

PHP_METHOD(Rows, nextPage)
{
  zval *timeout = NULL;
  php_driver_rows *self = PHP_DRIVER_GET_ROWS(getThis());

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &timeout) == FAILURE) {
    return;
  }

  if (!self->next_result) {
    if (!Z_ISUNDEF(self->future_next_page)) {
      php_driver_future_rows *future_rows = NULL;

      if (!instanceof_function(Z_OBJCE(self->future_next_page),
                               php_driver_future_rows_ce)) {
        zend_throw_exception_ex(php_driver_runtime_exception_ce, 0,
                                "Unexpected future instance.");
        return;
      }

      future_rows = PHP_DRIVER_GET_FUTURE_ROWS(&(self->future_next_page));

      if (php_driver_future_rows_get_result(future_rows, timeout) == FAILURE) {
        return;
      }

      self->next_result = php_driver_add_ref(future_rows->result);
    } else {
      const CassResult *result = NULL;
      CassFuture *future = NULL;

      if (self->result == NULL) {
        return;
      }

      ASSERT_SUCCESS(cass_statement_set_paging_state((CassStatement *) self->statement->data,
                                                     (const CassResult *) self->result->data));

      future = cass_session_execute((CassSession *) self->session->data,
                                    (CassStatement *) self->statement->data);

      if (php_driver_future_wait_timed(future, timeout) == FAILURE) {
        return;
      }

      if (php_driver_future_is_error(future) == FAILURE) {
        return;
      }

      result = cass_future_get_result(future);
      if (!result) {
        cass_future_free(future);
        zend_throw_exception_ex(php_driver_runtime_exception_ce, 0,
                                "Future doesn't contain a result.");
        return;
      }

      self->next_result = php_driver_new_ref((void *)result , free_result);

      cass_future_free(future);
    }
  }

  /* Always create a new rows object to avoid creating a linked list of
   * objects.
   */
  php_driver_rows_create(self, return_value);
}

PHP_METHOD(Rows, nextPageAsync)
{
  php_driver_rows *self = NULL;
  php_driver_future_rows *future_rows = NULL;

  if (zend_parse_parameters_none() == FAILURE)
    return;

  self = PHP_DRIVER_GET_ROWS(getThis());

  if (!Z_ISUNDEF(self->future_next_page)) {
    RETURN_ZVAL(&(self->future_next_page), 1, 0);
  }

  if (self->next_result) {
    php_driver_future_value *future_value;

    object_init_ex(&(self->future_next_page), php_driver_future_value_ce);
    future_value = PHP_DRIVER_GET_FUTURE_VALUE(&(self->future_next_page));

    php_driver_rows_create(self, &(future_value->value));
    RETURN_ZVAL(&(self->future_next_page), 1, 0);
  }

  if (self->result == NULL) {
    object_init_ex(return_value, php_driver_future_value_ce);
    return;
  }

  ASSERT_SUCCESS(cass_statement_set_paging_state((CassStatement *) self->statement->data,
                                                 (const CassResult *) self->result->data));


  object_init_ex(&(self->future_next_page), php_driver_future_rows_ce);
  future_rows = PHP_DRIVER_GET_FUTURE_ROWS(&(self->future_next_page));

  future_rows->statement = php_driver_add_ref(self->statement);
  future_rows->session = php_driver_add_ref(self->session);
  future_rows->future    = cass_session_execute((CassSession *) self->session->data,
                                                (CassStatement *) self->statement->data);

  RETURN_ZVAL(&(self->future_next_page), 1, 0);
}

PHP_METHOD(Rows, pagingStateToken)
{
  const char *paging_state;
  size_t paging_state_size;
  php_driver_rows* self = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_ROWS(getThis());

  if (self->result == NULL) return;

  ASSERT_SUCCESS(cass_result_paging_state_token((const CassResult *) self->result->data,
                                                &paging_state,
                                                &paging_state_size));
  RETURN_STRINGL(paging_state, paging_state_size);
}

PHP_METHOD(Rows, first)
{
  HashPosition pos;
  zval *entry;
  php_driver_rows* self = NULL;

  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }

  self = PHP_DRIVER_GET_ROWS(getThis());

  zend_hash_internal_pointer_reset_ex(Z_ARRVAL(self->rows), &pos);
  if (CASS_ZEND_HASH_GET_CURRENT_DATA(Z_ARRVAL(self->rows), entry)) {
    RETVAL_ZVAL(entry, 1, 0);
  }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_none, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetExists, ZEND_RETURN_VALUE, 1, _IS_BOOL, 0)
                ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetGet, ZEND_RETURN_VALUE, 1, CASS_COMPAT_IS_MIXED, 0)
                ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetSet, ZEND_RETURN_VALUE, 2, IS_VOID, 0)
                ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
                ZEND_ARG_TYPE_INFO(0, value, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_offsetUnset, ZEND_RETURN_VALUE, 1, IS_VOID, 0)
                ZEND_ARG_TYPE_INFO(0, offset, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_timeout, 0, ZEND_RETURN_VALUE, 1)
  ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_count, ZEND_RETURN_VALUE, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_current, ZEND_RETURN_VALUE, 0, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_key, ZEND_RETURN_VALUE, 0, CASS_COMPAT_IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_next, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_valid, ZEND_RETURN_VALUE, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_rewind, ZEND_RETURN_VALUE, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static zend_function_entry php_driver_rows_methods[] = {
  PHP_ME(Rows, __construct,      arginfo_none,    ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(Rows, count,            arginfo_count,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, rewind,           arginfo_rewind,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, current,          arginfo_current,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, key,              arginfo_key,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, next,             arginfo_next,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, valid,            arginfo_valid,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, offsetExists,     arginfo_offsetExists,  ZEND_ACC_PUBLIC)
  PHP_ME(Rows, offsetGet,        arginfo_offsetGet,  ZEND_ACC_PUBLIC)
  PHP_ME(Rows, offsetSet,        arginfo_offsetSet,     ZEND_ACC_PUBLIC)
  PHP_ME(Rows, offsetUnset,      arginfo_offsetUnset,  ZEND_ACC_PUBLIC)
  PHP_ME(Rows, isLastPage,       arginfo_none,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, nextPage,         arginfo_timeout, ZEND_ACC_PUBLIC)
  PHP_ME(Rows, nextPageAsync,    arginfo_none,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, pagingStateToken, arginfo_none,    ZEND_ACC_PUBLIC)
  PHP_ME(Rows, first,            arginfo_none,    ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static zend_object_handlers php_driver_rows_handlers;

static int
php_driver_rows_compare(zval *obj1, zval *obj2)
{
  ZEND_COMPARE_OBJECTS_FALLBACK(obj1, obj2);

  return Z_OBJ_HANDLE_P(obj1) != Z_OBJ_HANDLE_P(obj1);
}

static void
php_driver_rows_free(zend_object *object)
{
  php_driver_rows *self = php_driver_rows_object_fetch(object);;

  php_driver_del_ref(&self->result);
  php_driver_del_ref(&self->statement);
  php_driver_del_peref(&self->session, 1);
  php_driver_del_ref(&self->next_result);

  CASS_ZVAL_MAYBE_DESTROY(self->rows);
  CASS_ZVAL_MAYBE_DESTROY(self->next_rows);
  CASS_ZVAL_MAYBE_DESTROY(self->future_next_page);

  zend_object_std_dtor(&self->zval);

}

static zend_object *
php_driver_rows_new(zend_class_entry *ce)
{
  php_driver_rows *self =
      CASS_ZEND_OBJECT_ECALLOC(rows, ce);

  self->statement   = NULL;
  self->session     = NULL;
  self->result      = NULL;
  self->next_result = NULL;
  ZVAL_UNDEF(&(self->rows));
  ZVAL_UNDEF(&(self->next_rows));
  ZVAL_UNDEF(&(self->future_next_page));

  CASS_ZEND_OBJECT_INIT(rows, self, ce);
}

void php_driver_define_Rows()
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, PHP_DRIVER_NAMESPACE "\\Rows", php_driver_rows_methods);
  php_driver_rows_ce = zend_register_internal_class(&ce);
  zend_class_implements(php_driver_rows_ce, 3, zend_ce_iterator, zend_ce_arrayaccess, zend_ce_countable);
  php_driver_rows_ce->ce_flags     |= ZEND_ACC_FINAL;
  php_driver_rows_ce->create_object = php_driver_rows_new;

  memcpy(&php_driver_rows_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
  CASS_COMPAT_SET_COMPARE_HANDLER(php_driver_rows_handlers, php_driver_rows_compare);
  php_driver_rows_handlers.clone_obj = NULL;
}
