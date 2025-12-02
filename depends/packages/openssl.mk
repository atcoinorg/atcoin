package=openssl
$(package)_version=1.1.1w
$(package)_download_path=https://www.openssl.org/source/
$(package)_file_name=openssl-$($(package)_version).tar.gz
$(package)_sha256_hash=cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8

define $(package)_set_vars
$(package)_config_opts=no-tests no-ssl2 no-ssl3 no-weak-ssl-ciphers
endef

define $(package)_config_cmds
	./config $(strip $($(package)_config_opts))
endef

define $(package)_build_cmds
	$(MAKE)
endef

define $(package)_stage_cmds
	$(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
