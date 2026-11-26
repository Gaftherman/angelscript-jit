void main()
{
    string s;
    int64 sum = 0;
    for (int i = 0; i < 400000; i++)
    {
        s = "item" + i;
        sum += s.length();
        if (s.findFirst("9") >= 0) sum++;
    }
    print(sum);
}
