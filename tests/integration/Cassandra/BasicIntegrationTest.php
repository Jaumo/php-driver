<?php

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

namespace Cassandra;

/**
 * Basic/Simple integration test class to provide common integration test
 * functionality when a simple setup and teardown is required. This class
 * should be used for the majority of tests.
 */
abstract class BasicIntegrationTest extends \PHPUnit\Framework\TestCase {
    /**
     * Conversion value for seconds to milliseconds.
     */
    const SECONDS_TO_MILLISECONDS = 1000;
    /**
     * Conversion value for seconds to microseconds.
     */
    const SECONDS_TO_MICROSECONDS = 1000000;

    /**
     * Integration test instance (helper class).
     *
     * @var Integration
     */
    private $integration;
    /**
     * Handle for interacting with CCM.
     *
     * @var CCM
     */
    protected $ccm;
    /**
     * Number of nodes in data center one.
     *
     * @var int
     */
    protected $numberDC1Nodes = 1;
    /**
     * Replication factor override.
     *
     * @var int
     */
    protected $replicationFactor = -1;
    /**
     * Established cluster configuration.
     *
     * @var \Cassandra\Cluster
     */
    protected $cluster;
    /**
     * Connected database session.
     *
     * @var \Cassandra\Session
     */
    protected $session;
    /**
     * Version of Cassandra/DSE the session is connected to.
     *
     * @var string
     */
    protected $serverVersion;
    /**
     * Keyspace name being used for the test.
     *
     * @var string
     */
    protected $keyspaceName;
    /**
     * Table name prefix being used for the test.
     *
     * @var string
     */
    protected $tableNamePrefix;
    /**
     * Flag to determine if UDA/UDF functionality should be enabled.
     *
     * @var bool
     */
    protected $isUserDefinedAggregatesFunctions = false;
    /**
     * Share integration/cassandra setup across multiple test-methods in a testcase
     *
     * Has a positive performance impact
     *
     * @var Integration|null
     */
    protected static $sharedIntegration = null;

    public static function setUpBeforeClass(): void
    {
        // Do not share integration across test boundaries
        self::$sharedIntegration = null;
    }

    /**
     * Setup the database for the integration tests.
     */
    protected function setUp(): void {
        if (self::$sharedIntegration == null) {
            self::$sharedIntegration = new Integration(
                static::class,
                "",
                $this->numberDC1Nodes,
                0,
                $this->replicationFactor,
                false,
                false,
                $this->isUserDefinedAggregatesFunctions
            );
        }

        // Initialize the database and establish a connection
        $this->integration = self::$sharedIntegration;
        $this->ccm = $this->integration->ccm;
        $this->cluster = $this->integration->cluster;
        $this->session = $this->integration->session;
        $this->serverVersion = $this->integration->serverVersion;

        // Assign the keyspace and table name for the test
        $this->keyspaceName = strtolower($this->integration->keyspaceName);
        $this->tableNamePrefix = strtolower($this->getName(false));
    }

    /**
     * Teardown the database for the integration tests.
     */
    protected function tearDown(): void {
        unset($this->integration);
        unset($this->ccm);
        unset($this->session);

        // explicitly running the garbage collector after every test allows for quicker
        // (and less flaky) detection of gc related extension errors
        gc_collect_cycles();
    }

    protected function resetKeyspaceAfterEachTest() {
        self::$sharedIntegration = null;
    }
}
