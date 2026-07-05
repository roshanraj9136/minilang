fn is_prime(n: int) -> bool {
    if (n <= 1) {
        return false;
    }
    let i: int = 2;
    while (i * i <= n) {
        if (n % i == 0) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn main() {
    for (let n: int = 2; n <= 30; n = n + 1) {
        if (is_prime(n)) {
            print(n);
        }
    }
}
