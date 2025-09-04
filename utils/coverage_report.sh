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

llvm-cov show ./ng_test -instr-profile=coverage.profdata -format=html -ignore-filename-regex='catch2-src|ng\/test' -output-dir=reports/cov -show-line-counts-or-regions -Xdemangler c++filt -Xdemangler -n
llvm-cov export ./ng_test -instr-profile=coverage.profdata -ignore-filename-regex='catch2-src|ng\/test' -format=lcov >coverage.lcov
