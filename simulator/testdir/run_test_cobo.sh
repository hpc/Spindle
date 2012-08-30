TESTNAME=$1

# prepare
if (! [ -d run_test_${TESTNAME} ] ) 
then
    mkdir run_test_${TESTNAME}
fi


(

  cd run_test_${TESTNAME}
  
  
  LD_LIBRARY_PATH=/g/g92/frings1/LLNL/pynamic/benchmark3:$LD_LIBRARY_PATH
  LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
  export LD_LIBRARY_PATH
  
  LDCS_LOCATION=/tmp/myfifo
  export LDCS_LOCATION
  
  LDCS_NUMBER=7777
  export LDCS_NUMBER
  
#LD_DEBUG=libs
#export LD_DEBUG
  
  SION_DEBUG=debug/_debug_audit_client_mpi
  export SION_DEBUG
  
  rm _debug_sim*

  cp ../${TESTNAME}/test.dat ./searchlist.dat

  cp ../simulator.dat ./simulator.dat

  ln -sf ../lib ./lib

  #/usr/bin/srun -N 1 -n 5 --distribution block ../simulator_pipe_mpi_cobo

  echo "run test $TESTNAME ..."
  mpirun -np 5 ../../simulator_pipe_mpi_cobo 1> test.log 2>test.err  

  grep "CLIENT\[0\]" test.log > test_C0.log
  grep "CLIENT\[1\]" test.log > test_C1.log

  if diff test_C0.log  ../${TESTNAME}/test_C0.log > /dev/null
  then
      if diff test_C1.log ../${TESTNAME}/test_C1.log > /dev/null
      then
	  echo "     test_${TESTNAME}              ... OK"
	true
      else
	echo "     test_${TESTNAME}              ... FAILED (diff in client 0)"
	diff test_C0.log  ../${TESTNAME}/test_C0.log
    fi
  else
      echo "     test_${TESTNAME}              ... FAILED (diff in client 1)"
      diff test_C1.log ../${TESTNAME}/test_C1.log
  fi
  
)

