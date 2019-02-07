./simpsh --rdonly pg98.txt --creat --wronly test3_err.txt --creat --rdwr test3_out.txt  --profile --command 0 2 1 cat --wait --command 2 2 1 tr [:lower:] [:upper:] --wait --command 2 2 1 tr [:upper:] [:lower:]  --wait >test3times_out.txt
if [ $? == 0 ]
then
  echo "Test 1 Passed"
else
  echo "Test 1 Failed"
fi

./simpsh --rdonly pg98.txt --creat --wronly test3_err.txt --creat --rdwr test3_out.txt  --profile --command 0 2 1 cat --wait --command 2 2 1 tr [:lower:] [:upper:] --wait --command 2 2 1 tr [:upper:] [:lower:]  --wait >test3times_out.txt
if [ $? == 0 ]
then
  echo "Test 2 Passed"
else
  echo "Test 2 Failed"
fi
