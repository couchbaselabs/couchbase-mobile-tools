# cbl-log packaging

ifeq ($(strip $(VERSION)),)
REL_VERSION  = 1.0.0
REVISION     = 9999
else
REL_VERSION  = $(VERSION)
REVISION = $(BLD_NUM)
endif

NAME=cbl-log
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
package: clean rpm deb

rpm:
	$(FPM_COMMON_OPTIONS) -t rpm --iteration $(REVISION)_linux bin LICENSE.txt

deb:
	$(FPM_COMMON_OPTIONS) -t deb --iteration $(REVISION)-linux bin LICENSE.txt

clean: 
	rm -f $(NAME)*.rpm $(NAME)*.deb
