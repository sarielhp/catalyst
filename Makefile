SUBDIRS := wait10 wait_random wait_random_busy

STR:=$(datestrext)
HST:=$(hostname -s)


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
	echo  "Time/date: ${STR} from ${HST}"
#git commit -a --allow-empty-message -m "Time/date: $(STR) from $(HST)"
#	git push


catalyst: catalyst.C
	g++ -std=c++17 -Wall -o catalyst catalyst.C
