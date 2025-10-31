/*
 * Jimmy Paputto 2021
 */

#ifndef GENERIC_SINGLETON_HPP_
#define GENERIC_SINGLETON_HPP_


template<typename T>
class GenericSingleton
{
public:
    static T& instance()
    {
        static T instance;
        return instance;
    }

    GenericSingleton(GenericSingleton const&) = delete;
    void operator=(GenericSingleton const&) = delete;

protected:
    explicit GenericSingleton() {}
};

#endif  // GENERIC_SINGLETON_HPP_
