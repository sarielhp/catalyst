SUBDIRS := wait10 wait_random wait_random_busy

define FOREACH
    for DIR in $(SUBDIRS); do \
        $(MAKE) -C $$DIR $(1); \
    done
endef

.PHONY: build
build: catalyst
	$(call FOREACH,build)
#	g++ -Wall -o catalyst catalyst.C

.PHONY: clean
clean:
	rm -f catalyst success.txt
	rm -r -f work/
	mkdir work 
	$(call FOREACH,clean)

catalyst: catalyst.C
	g++ -std=c++17 -Wall -o catalyst catalyst.C
