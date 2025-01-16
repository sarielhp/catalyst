SUBDIRS := wait10 wait_random wait_random_busy

STR := $(shell datestrext)

HST:=$(shell hostname -s)

V := "Time/date: ${STR} from $(HST)"


define FOREACH
    for DIR in $(SUBDIRS); do \
        $(MAKE) -C $$DIR $(1); \
    done
endef

.PHONY: build git_push
build: catalyst
	$(call FOREACH,build)
#	g++ -Wall -o catalyst catalyst.C

.PHONY: clean
clean:
	rm -f catalyst success.txt
	rm -r -f work/
	mkdir work 
	$(call FOREACH,clean)

git_push: clean
	rm -r -f work
#	echo " ---- "#
#	echo ${STR}#
#	echo $(V)
#	echo	$(shell git commit -a --allow-empty-message -m "$(V)")
	$(shell echo git commit -a -m \""$(V)"\")
	$(shell git push)


catalyst: catalyst.C
	g++ -std=c++17 -Wall -o catalyst catalyst.C
