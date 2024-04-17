#ifndef UNROLLED_MATRIX_HPP_W3RE9FXB
#define UNROLLED_MATRIX_HPP_W3RE9FXB

#include <algorithm>
#include <concepts>
#include <utility>
#include <vector>
#include <ranges>

template <std::default_initializable T>
class UnrolledMatrix
{
public:
    using Element_type = T;

    UnrolledMatrix() = default;

    UnrolledMatrix(
        ssize_t width,
        ssize_t height
    )
        : m_mat(width * height)
        , m_width{ width }
        , m_height{ height }
    {
    }

    UnrolledMatrix(
        ssize_t      width,
        ssize_t      height,
        Element_type init
    )
        requires std::copy_constructible<Element_type>
        : m_mat(width * height, init)
        , m_width{ width }
        , m_height{ height }
    {
    }

    template <std::invocable<Element_type&> Func>
    void apply(Func&& func)
    {
        for (auto& elem : m_mat) {
            std::forward<Func>(func)(elem);
        }
    }

    template <typename TT, std::invocable<Element_type&, TT&> Func>
    void apply(const UnrolledMatrix<TT>& other, Func&& func)
    {
        for (auto i : std::views::iota(0ull, (std::size_t)std::min(length(), other.length()))) {
            std::forward<Func>(func)(m_mat[i], other.base()[i]);
        }
    }

    template <std::invocable<Element_type&> Func>
    UnrolledMatrix transform(Func&& func) const
    {
        auto copy = *this;
        copy.apply(std::forward<Func>(func));
        return copy;
    }

    template <typename TT, std::invocable<Element_type&, TT&> Func>
    UnrolledMatrix transform(const UnrolledMatrix<TT>& other, Func&& func) const
    {
        auto copy = *this;
        copy.apply(other, std::forward<Func>(func));
        return copy;
    }

    Element_type& get(ssize_t col, ssize_t row)
    {
        if (col < 0 || row < 0 || col >= m_width || row >= m_height) {
            throw std::range_error{ "out of bound" };
        }

        ssize_t idx{ row * m_width + col };
        return m_mat[idx];
    }

    const Element_type& get(ssize_t col, ssize_t row) const
    {
        if (col < 0 || row < 0 || col >= m_width || row >= m_height) {
            throw std::out_of_range{ "out of bound" };
        }

        ssize_t idx{ row * m_width + col };
        return m_mat[idx];
    }

    Element_type& operator()(ssize_t col, ssize_t row)
    {
        return get(col, row);
    }
    const Element_type& operator()(ssize_t col, ssize_t row) const
    {
        return get(col, row);
    }

    // return pair of width, height
    std::pair<ssize_t, ssize_t> dimension() const
    {
        return { m_width, m_height };
    }

    ssize_t width() const
    {
        return m_width;
    }
    ssize_t height() const
    {
        return m_height;
    }
    ssize_t length() const
    {
        return m_width * m_height;
    }

    const auto& data() const
    {
        return m_mat;
    }
    auto& base()
    {
        return m_mat;
    }

    void swap(UnrolledMatrix& other)
    {
        std::swap(m_width, other.m_width);
        std::swap(m_height, other.m_height);
        std::swap(m_mat, other.m_mat);
        // std::swap(*this, other);    // can i do this instead?
    }

    void clear()
    {
        m_mat.clear();
    }

private:
    ssize_t                   m_width  = 0;
    ssize_t                   m_height = 0;
    std::vector<Element_type> m_mat;
};

#endif
