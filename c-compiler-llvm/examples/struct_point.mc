// examples/struct_point.mc

struct Point {
    int x;
    int y;
};

enum Direction {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

struct Point makeOrigin() {
    struct Point p;
    p.x = 0;
    p.y = 0;
    return p;
}

void move(struct Point *p, int direction, int amount) {
    if (direction == NORTH) {
        p->y = p->y + amount;
    } else if (direction == EAST) {
        p->x = p->x + amount;
    } else if (direction == SOUTH) {
        p->y = p->y - amount;
    } else if (direction == WEST) {
        p->x = p->x - amount;
    }
}

int main() {
    struct Point p = makeOrigin();
    move(&p, EAST, 5);
    move(&p, NORTH, 3);
    printf("(%d, %d)\n", p.x, p.y);
    return 0;
}
