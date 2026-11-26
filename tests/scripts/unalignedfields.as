class Shape
{
    int id; double w;
    Shape(int i, double v) { id = i; w = v; }
    double area() { return w * w; }
}
void main()
{
    Shape s(3, 2.5);
    double total = 0;
    for (int i = 0; i < 4; i++) total += s.area();
    print(total);
}
