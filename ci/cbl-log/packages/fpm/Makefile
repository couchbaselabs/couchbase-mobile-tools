# cbl-log packaging

ifeq ($(strip $(VERSION)),)
REL_VERSION      = 1.0.0
REVISION     = 9999
else
REL_VERSION  = $(VERSION)
REVISION = $(BLD_NUM)
endif

NAME=couchbase-lite-log
VENDOR='Couchbase Inc.'
INSTALL_PREFIX=/opt/cbl-log
PACKAGE_DESCRIPTION='The cbl-log Tool'
URL='https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/README.cbl-log.md'
POST_UNINSTALL_SCRIPT=cbl-log-post-remove.sh

EXCLUDE_FILES='*logtest'
FPM_COMMON_OPTIONS=fpm -s dir \
		-n $(NAME) \
		-v $(REL_VERSION) \
		--prefix $(INSTALL_PREFIX) \
		--chdir install \
		--description $(PACKAGE_DESCRIPTION) \
		--url $(URL) \
		--vendor $(VENDOR) \
		--after-remove $(POST_UNINSTALL_SCRIPT) \
		--exclude $(EXCLUDE_FILES) --debug

.PHONY: package
package: clean rpm deb1804 deb1604

rpm:
	$(FPM_COMMON_OPTIONS) -t rpm --iteration $(REVISION)_centos7 bin lib LICENSE.txt
	mv $(NAME)-$(REL_VERSION)*.rpm $(NAME)-$(REL_VERSION)-$(REVISION)-centos7.x86_64.rpm

deb1804:
	$(FPM_COMMON_OPTIONS) -t deb --iteration $(REVISION)-ubuntu18.04 --depends "libc++1 >= 3.9" --depends libc++abi1 bin LICENSE.txt

deb1604:
	$(FPM_COMMON_OPTIONS) -t deb --iteration $(REVISION)-ubuntu16.04 bin lib LICENSE.txt

clean: 
	rm -f $(NAME)*.rpm $(NAME)*.deb
