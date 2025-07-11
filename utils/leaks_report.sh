mkdir -p reports/
leaks --atExit >reports/LeaksReport.txt -- ./ng_test
