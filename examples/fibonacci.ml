fn fibonacci(n: int) -> int {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

fn main() {
    let i: int = 0;
    while (i < 10) {
        print(fibonacci(i));
        i = i + 1;
    }
}
