DEBUG=DEBUG

.PHONY: all lib cache auditclient auditserver client server sion_debug simulator


all: auditclient auditserver client simulator cobotest

auditclient: lib
	@echo "++++++++++ AUDITCLIENT ++++++++++++++++++++++++++++++"
	@if test -d auditclient; then (cd auditclient; $(MAKE) all ); fi

auditserver: lib cache cobo
	@echo "++++++++++ AUDITSERVER ++++++++++++++++++++++++++++++"
	@if test -d auditserver; then (cd auditserver; $(MAKE) all ); fi

client: lib
	@echo "++++++++++ CLIENT ++++++++++++++++++++++++++++++"
	@if test -d client; then (cd client; $(MAKE) all ); fi

simulator: lib cache cobo
	@echo "++++++++++ SIMULATOR ++++++++++++++++++++++++++++++"
	@if test -d simulator; then (cd simulator; $(MAKE) all ); fi

server: lib
	@echo "++++++++++ SERVER ++++++++++++++++++++++++++++++"
	@if test -d server; then (cd server; $(MAKE) all ); fi

lib: sion_debug
	@echo "++++++++++ LIB ++++++++++++++++++++++++++++++"
	@if test -d lib; then (cd lib; $(MAKE) all ); fi

sion_debug: 
	@echo "++++++++++ sion_debug ++++++++++++++++++++++++++++++"
	@if test -d tools/sion_debug; then (cd tools/sion_debug; $(MAKE) all ); fi

cobo: sion_debug
	@echo "++++++++++ cobo ++++++++++++++++++++++++++++++"
	@if test -d tools/cobo/src; then (cd tools/cobo/src; $(MAKE) all ); fi

cobotest: sion_debug auditserver
	@echo "++++++++++ cobotest ++++++++++++++++++++++++++++++"
	@if test -d tools/cobo/test; then (cd tools/cobo/test; $(MAKE) all ); fi

launchmon: auditserver sion_debug
	@echo "++++++++++ LAUNCHMON ++++++++++++++++++++++++++++++"
	@if test -d launchmon; then (cd launchmon; $(MAKE) all ); fi

cache: 
	@echo "++++++++++ CACHE ++++++++++++++++++++++++++++++"
	@if test -d cache; then (cd cache; $(MAKE) all ); fi



clean:
	@if test -d lib; then ( cd lib; $(MAKE) clean ); fi
	@if test -d cache; then ( cd cache; $(MAKE) clean ); fi
	@if test -d auditserver; then ( cd auditserver; $(MAKE) clean ); fi
	@if test -d auditclient; then ( cd auditclient; $(MAKE) clean ); fi
	@if test -d simulator; then ( cd simulator; $(MAKE) clean ); fi
	@if test -d client; then ( cd client; $(MAKE) clean ); fi
	@if test -d server; then ( cd server; $(MAKE) clean ); fi
	@if test -d tools/sion_debug; then (cd tools/sion_debug; $(MAKE) clean ); fi
	@if test -d tools/cobo/src; then (cd tools/cobo/src; $(MAKE) clean ); fi
	@if test -d tools/cobo/test; then (cd tools/cobo/test; $(MAKE) clean ); fi
	@if test -d launchmon; then ( cd launchmon; $(MAKE) clean ); fi
