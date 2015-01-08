#define __PRESURE 10
class Point {
    public:
    int x, y, z;

    public:
    Point(void);
    Point(int x, int y, int z);
    bool operator==(Point);
    bool operator!=(Point);

};

class TouchScreen {
    private:
    unsigned char _yp, _ym, _xm, _xp;

    public:
    TouchScreen(unsigned char xp, unsigned char yp, unsigned char xm, unsigned char ym);
    bool isTouching(void);
    Point getPoint();

};
