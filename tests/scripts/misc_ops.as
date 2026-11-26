// Instructions that used to fall back to the VM: FuncPtr (funcdef values), SwapPtr, ChkRefS, ClrHi and LdGRdR4.
funcdef int Op(int);
int twice(int x) { return x * 2; }
int inc(int x) { return x + 1; }

int g_counter = 5;
bool g_flag = true;
class Box { Box@ next; int v = 3; }
string g_text = "abc";

int apply(Op@ f, int x) { return f(x); }

void main()
{
    Op@ a = twice;
    Op@ b = inc;
    print(apply(a, 10));
    print(apply(b, 10));
    int local = g_counter;
    g_counter = local + 1;
    print(g_counter);
    bool x = g_flag && local > 2;
    print(x ? "yes" : "no");
    Box@ head = Box();
    @head.next = Box();
    head.next.v = 9;
    print(head.next.v);
    Box@ other = head;
    @other = head.next;
    print(other.v);
    string s = g_text + "d";
    s += g_text;
    print(s);
    array<string> parts = {"a", "b"};
    print(parts[0] + parts[1] + g_text);
    try { Box@ empty = Box(); print(empty.next.v); } catch { print("null handle"); }
}
