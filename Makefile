PHP_VERSION=8.1-rc
DOCKER_TAG=php-ext-cassandra

DEV_IMAGE=$(DOCKER_TAG)/dev:$(PHP_VERSION)
FULL_IMAGE=$(DOCKER_TAG)/full:$(PHP_VERSION)

build-dev-image:
	@docker build -f Dockerfile --build-arg PHP_VERSION=$(PHP_VERSION) -t $(DEV_IMAGE) .

build-full-image: build-dev-image
	@docker build -f Dockerfile.full --build-arg PHP_VERSION=$(PHP_VERSION) --build-arg DOCKER_TAG=$(DOCKER_TAG) -t $(FULL_IMAGE) .

shell-dev: build-dev-image
	@docker run --rm -it  -v $(shell pwd):/code $(DEV_IMAGE) bash

shell-full: build-full-image
	@docker run --rm -it $(FULL_IMAGE)  bash 

test:
	@docker run --rm -it $(FULL_IMAGE) composer test	

test-unit:
	@docker run --rm -it $(FULL_IMAGE) bin/phpunit tests/unit

test-integration:
	@docker run --rm -it $(FULL_IMAGE) bin/phpunit tests/integration
	@docker run --rm -it $(FULL_IMAGE) bin/behat
