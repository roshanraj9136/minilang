fn factorial(n: int) -> int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

fn main() {
    for (let i: int = 1; i <= 10; i = i + 1) {
        print(factorial(i));
    }
}
