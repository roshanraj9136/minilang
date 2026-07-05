fn gcd(a: int, b: int) -> int {
    while (b != 0) {
        let temp: int = b;
        b = a % b;
        a = temp;
    }
    return a;
}

fn main() {
    print(gcd(48, 18));
    print(gcd(100, 75));
    print(gcd(17, 13));
}
