noinst_PROGRAMS = libgenerator test_driver test_driver_libs

ABS_TEST_DIR = $(abspath $(top_builddir)/testsuite)
BUILT_SOURCES = libtest10.so libtest50.so libtest100.so libtest500.so libtest1000.so libtest2000.so libtest4000.so libtest6000.so libtest8000.so libtest10000.so libdepC.so libdepB.so libdepA.so libcxxexceptB.so libcxxexceptA.so libtestoutput.so libfuncdict.so runTests run_driver run_driver_rm

libgenerator_SOURCES = libgenerator.c

test_driver_SOURCES = test_driver.c
test_driver_CFLAGS = -DLPATH=$(top_builddir)/testsuite $(MPI_CFLAGS)
test_driver_LDADD = -ldl -ltestoutput -lfuncdict
test_driver_LDFLAGS = -Wl,-E -L$(top_builddir)/testsuite $(MPI_CLDFLAGS)

test_driver_libs_SOURCES = test_driver.c
test_driver_libs_CFLAGS = -DLPATH=$(top_builddir)/testsuite $(MPI_CFLAGS)
test_driver_libs_LDADD = -ltest10 -ltest50 -ltest100 -ltest500 -ltest1000 -ltest2000 -ltest4000 -ltest6000 -ltest8000 -ltest10000 -ldepA -lcxxexceptA -ldl -ltestoutput -lfuncdict -ldepB -ldepC -lcxxexceptB
test_driver_libs_LDFLAGS = -Wl,-E -L$(top_builddir)/testsuite $(MPI_CLDFLAGS)

REGLIB_SRC = $(srcdir)/registerlib.c
LD_FUNCDICT = -L$(top_builddir)/testsuite -lfuncdict

libtest10.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest10.c 10 t10

libtest50.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest50.c 50 t50

libtest100.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest100.c 100 t100

libtest500.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest500.c 500 t500

libtest1000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest1000.c 1000 t1000

libtest2000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest2000.c 2000 t2000

libtest4000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest4000.c 4000 t4000

libtest6000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest6000.c 6000 t6000

libtest8000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest8000.c 8000 t8000

libtest10000.c: libgenerator
	$(AM_V_GEN)./libgenerator libtest10000.c 10000 t10000

libtest10.so: libtest10.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t10_calc $(LD_FUNCDICT)

libtest50.so: libtest50.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t50_calc $(LD_FUNCDICT)

libtest100.so: libtest100.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t100_calc $(LD_FUNCDICT)

libtest500.so: libtest500.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t500_calc $(LD_FUNCDICT)

libtest1000.so: libtest1000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t1000_calc $(LD_FUNCDICT)

libtest2000.so: libtest2000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t2000_calc $(LD_FUNCDICT)

libtest4000.so: libtest4000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t4000_calc $(LD_FUNCDICT)

libtest6000.so: libtest6000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t6000_calc $(LD_FUNCDICT)

libtest8000.so: libtest8000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t8000_calc $(LD_FUNCDICT)

libtest10000.so: libtest10000.c $(REGLIB_SRC) libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $^ -shared -DSO_NAME=$@ -DFUNC_NAME=t10000_calc $(LD_FUNCDICT)

libdepA.so: libdepA.c $(REGLIB_SRC) libdepB.so libfuncdict.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $< $(REGLIB_SRC) -shared -L. -ldepB -DSO_NAME=$@ -DFUNC_NAME=depA_calc  $(LD_FUNCDICT)

libdepB.so: libdepB.c libdepC.so
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $< -shared -L. -ldepC

libdepC.so: libdepC.c
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $< -shared

libcxxexceptA.so: libcxxexceptA.cc $(REGLIB_SRC) libcxxexceptB.so libfuncdict.so
	$(AM_V_CCLD)$(CXX) -o $@ -fPIC -Wall $< $(REGLIB_SRC) -shared -L. -lcxxexceptB -DSO_NAME=$@ -DFUNC_NAME=cxxexceptA_calc $(LD_FUNCDICT)

libcxxexceptB.so: libcxxexceptB.cc
	$(AM_V_CCLD)$(CXX) -o $@ -fPIC -Wall $< -shared

libtestoutput.so: testoutput.c
	$(AM_V_CCLD)$(CXX) -o $@ -fPIC -Wall $< -shared

libfuncdict.so: funcdict.c
	$(AM_V_CCLD)$(CC) -o $@ -fPIC -Wall $< -shared

runTests: $(srcdir)/runTests_template
	$(AM_V_GEN) cp $(srcdir)/runTests_template $(top_builddir)/testsuite/runTests
	@chmod 700 $(top_builddir)/testsuite/runTests

run_driver: $(srcdir)/run_driver_template $(top_builddir)/Makefile
	@rm -f ./run_driver
	$(AM_V_GEN)$(SED) -e s,SPINDLE_EXEC,$(bindir)/spindle,g\;s,TEST_RUN_DIR,$(ABS_TEST_DIR),g < $(srcdir)/run_driver_template > $(top_builddir)/testsuite/run_driver
	@chmod 700 $(top_builddir)/testsuite/run_driver

run_driver_rm: $(srcdir)/run_driver_$(TESTRM) $(top_builddir)/Makefile
	$(AM_V_GEN)cp $(srcdir)/run_driver_$(TESTRM) $(top_builddir)/testsuite/run_driver_rm
	@chmod 700 $(top_builddir)/testsuite/run_driver_rm


CLEANFILES = libtest10.c libtest10.so libtest50.c libtest50.so libtest100.c libtest100.so libtest500.c libtest500.so libtest1000.c libtest1000.so libtest2000.c libtest2000.so libtest4000.c libtest4000.so libtest6000.c libtest6000.so libtest8000.c libtest8000.so libtest10000.c libtest10000.so libdepA.so libdepB.so libdepC.so libcxxexceptA.so libcxxexceptB.so libtestoutput.so libfuncdict.so runTests run_driver run_driver_rm

