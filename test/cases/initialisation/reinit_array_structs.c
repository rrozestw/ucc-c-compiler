// RUN: %layout_check %s
// 1, 2, 3, 4, 5, 6
struct S2 {
	int x, y;
} ent1[3] = {
	[2].y=6, [2].x=5,
	[1].y=4, [1].x=3,
	[0].y=2, [0].x=1
};
