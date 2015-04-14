template<int... Nums>
struct Sequence { };

template<int N, int... Rest>
struct IndexSequence : IndexSequence<N - 1, N - 1, Rest...> { };

template<int... Nums>
struct IndexSequence<0, Nums...>
{
    typedef Sequence<Nums...> seq;
};