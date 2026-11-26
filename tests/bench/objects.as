interface Shape { double area(); }
class Rect : Shape { double w, h; Rect(double a, double b) { w = a; h = b; } double area() { return w * h; } }
class Circle : Shape { double r; Circle(double a) { r = a; } double area() { return 3.14159 * r * r; } }
void main()
{
    array<Shape@> shapes;
    for (int i = 0; i < 64; i++)
    {
        if (i % 2 == 0) shapes.insertLast(Rect(i, 2.0));
        else shapes.insertLast(Circle(i));
    }
    double total = 0;
    for (int round = 0; round < 60000; round++)
        for (uint i = 0; i < shapes.length(); i++) total += shapes[i].area();
    print(total);
}
