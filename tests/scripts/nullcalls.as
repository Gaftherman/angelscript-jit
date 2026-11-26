// A method call through a null handle raises the same script exception on the VM and with the JIT, for virtual
// methods and for interface methods, and the arguments are released.
class Base { void hello(const string &in who) { print("base " + who); } }
class Derived : Base { void hello(const string &in who) override { print("derived " + who); } }
interface Speaker { void say(const string &in what); }
class Parrot : Speaker { void say(const string &in what) { print("parrot " + what); } }

void viaBase()
{
    Base@ b = null;
    b.hello("x" + 1);
}

void viaInterface()
{
    Speaker@ s = null;
    s.say("y" + 2);
}

void callThem()
{
    Base@ b = Derived();
    b.hello("ok");
    Speaker@ s = Parrot();
    s.say("ok");
}

void main()
{
    callThem();
    try { viaBase(); } catch { print("caught virtual: "); }
    try { viaInterface(); } catch { print("caught interface: "); }
    print("end");
}
