./ng_test

mkdir -p reports/cov

# Check if is macos (Darwin)
if [[ `uname -a` == *"Darwin"* ]]; then
    echo "Configuring macos environment variables"
    echo "Using Homebrew LLVM distribution"

    export PATH="/opt/homebrew/opt/llvm/bin/:$PATH"
fi

llvm-profdata merge -sparse default.profraw -o default.profdata

llvm-cov show ./ng_test -instr-profile=default.profdata -format=html -output-dir=reports/cov -show-line-counts-or-regions -Xdemangler c++filt -Xdemangler -n

open reports/cov/index.html
