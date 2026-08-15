if [ -d build ]; then
    echo "To Build folder"
    cd build/
fi

./ng_test

mkdir -p reports/cov

# Check if is macos (Darwin)
if [[ `uname -a` == *"Darwin"* ]]; then
    echo "Configuring macos environment variables"
    echo "Using Homebrew LLVM distribution"

    export PATH="/opt/homebrew/opt/llvm/bin/:$PATH"
fi

echo "Show llvm-cov runnning data"

llvm-profdata merge -sparse default.profraw -o coverage.profdata

llvm-cov show ./ng_test -instr-profile=coverage.profdata -format=html -ignore-filename-regex='vendored|test' -output-dir=reports/cov -show-line-counts-or-regions -Xdemangler c++filt -Xdemangler -n
llvm-cov export ./ng_test -instr-profile=coverage.profdata -ignore-filename-regex='vendored|test' -format=lcov >coverage.lcov

# Coverage gate: the pipeline sources must stay above 70% line coverage
# (llvm-cov, ignoring vendored/ and test/).
COVERAGE_LINE=$(llvm-cov report ./ng_test -instr-profile=coverage.profdata -ignore-filename-regex='vendored|test' -summary-only 2>/dev/null | tail -1 | awk '{print $NF}' | tr -d '%')
if [ -n "$COVERAGE_LINE" ] && [ "$(echo "$COVERAGE_LINE < 70" | bc)" = "1" ]; then
    echo "coverage gate failed: line coverage $COVERAGE_LINE% < 70%"
    exit 1
fi
echo "coverage gate passed: line coverage ${COVERAGE_LINE}%"
