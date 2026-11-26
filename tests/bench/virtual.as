class Base { int v; Base(int x) { v = x; } int get() { return v; } }
class Derived : Base { Derived(int x) { super(x); } int get() override { return v * 2; } }
void main()
{
    array<Base@> items;
    for (int i = 0; i < 64; i++) { if (i % 2 == 0) items.insertLast(Base(i)); else items.insertLast(Derived(i)); }
    int64 total = 0;
    for (int round = 0; round < 60000; round++)
        for (uint i = 0; i < items.length(); i++) total += items[i].get();
    print(total);
}
