// Test: align_func_proto_paren with mixed empty/comment/PP lines

class ComplexAPI {
public:
    ComplexAPI();
    explicit ComplexAPI(int value);
    ComplexAPI(const ComplexAPI& other) = default;
    virtual ~ComplexAPI();

    VeryLongReturnType complex_func_one();
    unsigned int       complex_func_two(int a, int b) const;
#ifdef PLATFORM_A
    // Platform specific function
    double*            complex_func_three(const std::string& name);

    // Another comment
    short              complex_func_four(const Point& p,
                                         std::size_t count);
#endif
#ifdef PLATFORM_B
    // Multiple lines here
    long int complex_func_five();
    // Comment line 1
    // Comment line 2
    // Comment line 3
    unsigned long complex_func_six(int x) noexcept;


    // Two empty + comment
    virtual int             complex_func_seven(int x) = 0;
#endif

    void                    final_func_eight();

    ComplexAPI&             operator=(const ComplexAPI& other);
    bool                    operator==(const ComplexAPI& other) const;
    int                     operator()(int x) const;
    template <typename T> T convert(const T& value) const;
    static ComplexAPI*      create();
};

class AnotherAPI
{
public:
    VeryLongReturnType another_func_nine();


    int                short_ten(int);

    VeryLongReturnType long_named_func_eleven(std::function<void(int)> callback);
#if 1
#if 1
#if 1
    int twelve();
#endif
#endif
#endif

    VeryLongReturnType extra_long_func_thirteen();
    // cmt 1
    // cmt 2
    // cmt 3
    // cmt 4
    int fn_fourteen();

    // Open parenthesis on the next line: not aligned
    int
         fifteen
        (int a);
    char sixteen(int a);

    // Single-line functions (align_single_line_func)
    AnotherAPI() {}
    explicit AnotherAPI(int v) : value(v) {}
    int get_value() const { return value; }
    void set_value(int v) { value = v; }
    std::size_t size() const;

    struct Nested {
        Nested();
        ~Nested();
        int         nested_func(int a);
        const char* nested_name() const;
    };

private:
    int value;
};

namespace detail {
VeryLongReturnType                ns_func_one(int a);
int                               ns_func_two(int a,
                                              int b);
template <class T> std::vector<T> ns_func_three(const T& v);
}

class Derived : public ComplexAPI
{
public:
    // Delegating constructors and base class initializers
    Derived() : Derived(0) {}
    Derived(int v) : ComplexAPI(v), count(v) {}
    Derived(const Derived& other)
        : ComplexAPI(other)
        , count(other.count)
    {}
    ~Derived() override; // destructor
    int           value() const; // value
    T*            get<T>(int index); // template arguments
    void          reset(); // reset

    // A constructor between two groups of functions
    int           first_one(int a);
    Derived(double d);
    unsigned long second_one(int a);

private:
    int count;
};

// Out-of-class definitions
Derived::Derived(double d) : ComplexAPI(static_cast<int>(d)), count(0) {}
Derived::~Derived() {}
int Derived::value() const { return count; }
void Derived::reset() { count = 0; }

// Operators that look like pointer operators, and several functions on a line
struct Vec
{
    Vec&   operator+=(const Vec& o);
    Vec    operator*(double k) const;
    Vec    operator&(const Vec& o) const;
    bool   operator&&(const Vec& o) const;
    Vec    operator^(const Vec& o) const;
    Vec*   operator->();
    double dot(const Vec& o) const;
    int    g(); int hh();
    Vec(int); Vec(long, long);
    Vec() :
        Vec(0) {}
};

// Spaces before the open parenthesis, a comment before it, and a constructor
// followed by a function on the same line
struct Spaced
{
    int           spaced  (int a);
    long          more_spaced     (int a);
    short         commented /* c */ (int a);
    Spaced(); int after_ctor(int a);
    ~Spaced();
};

// The constructors have their own line skip budget
class Budget
{
public:
    Budget();

    int first();

    explicit Budget(int a);

    unsigned long second();
    Budget(const Budget& o);


    long third();
};

// Comments between a name and its open parenthesis
struct Commented
{
    int  a /* c */(int);
    int  abcdefghijkl(int);
    int  abcdefghijklmno(int);
    long k /* multi
              line */(int);
    int  verylongname(int);
};

// Constructor calls that the tokenizer can take for constructors
struct Point
{
    Point(int x);
    Point origin = Point(0);
    explicit Point(long x);
    Point* copy = new Point(5);
    Point(const Point& p);
};
