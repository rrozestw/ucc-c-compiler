#define quote(x) #    x

quote(yo)

#define tim(...) timothy("" # __VA_ARGS__, __VA_ARGS__, 0)
tim(hello     ,     there);
