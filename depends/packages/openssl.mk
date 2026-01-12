package=openssl
$(package)_version=3.0.18
$(package)_download_path=https://github.com/openssl/openssl/releases/download/openssl-$($(package)_version)/
$(package)_file_name=openssl-$($(package)_version).tar.gz
$(package)_sha256_hash=d80c34f5cf902dccf1f1b5df5ebb86d0392e37049e5d73df1b3abae72e4ffe8b

define $(package)_set_vars
  ifeq ($(host_os),linux)
    ifeq ($(host_arch),x86_64)
      $(package)_config_opts+=linux-x86_64
    endif
    ifeq ($(host_arch),x86)
      $(package)_config_opts+=linux-x86
    endif
    ifeq ($(host_arch),arm64)
      $(package)_config_opts+=linux-aarch64
    endif
  endif

  ifeq ($(host_os),macos)
    ifeq ($(host_arch),x86_64)
      $(package)_config_opts+=darwin64-x86_64-cc
    endif
    ifeq ($(host_arch),arm64)
      $(package)_config_opts+=darwin64-arm64-cc
    endif
  endif

  ifeq ($(host_os),mingw32)
    ifeq ($(host_arch),x86_64)
      $(package)_config_opts+=mingw64
      $(package)_config_opts+=--cross-compile-prefix=x86_64-w64-mingw32-
    endif
    ifeq ($(host_arch),i686)
      $(package)_config_opts+=mingw
      $(package)_config_opts+=--cross-compile-prefix=i686-w64-mingw32-
    endif
  endif

  $(package)_config_opts+=--prefix=$(host_prefix)
  $(package)_config_opts+=--libdir=lib
  $(package)_config_opts+=no-asm
  $(package)_config_opts+=no-tests
  $(package)_config_opts+=no-ssl3
  $(package)_config_opts+=no-shared
endef

define $(package)_config_cmds
  ./Configure $(strip $($(package)_config_opts))
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install_sw
endef
