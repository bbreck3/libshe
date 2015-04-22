#include <cassert>
#include <random>

#include <iostream>
using std::cout;
using std::endl;

#include "she.hpp"
#include "defs.hpp"

using std::random_device;
using std::vector;

using boost::multiprecision::mpz_int;


namespace she
{
    ParameterSet::ParameterSet(unsigned int lambda,
                               unsigned int rho,
                               unsigned int eta,
                               unsigned int gamma,
                               unsigned int seed) :
      security(lambda),
      noise_size_bits(rho),
      private_key_size_bits(eta),
      ciphertext_size_bits(gamma),
      oracle_seed(seed)
    {};

    ParameterSet::ParameterSet() :
      security(1),
      noise_size_bits(1),
      private_key_size_bits(1),
      ciphertext_size_bits(1),
      oracle_seed(1)
    {};

    const ParameterSet
    ParameterSet::generate_parameter_set(unsigned int security, unsigned int circuit_mult_size, unsigned int seed)
    {
        assert(security > 0);
        unsigned int rho = 2 * security,
                     eta = security * security + security * circuit_mult_size,
                     gamma = eta * eta * circuit_mult_size;

        return { security, rho, eta, gamma, seed };
    }

    bool ParameterSet::operator==(const ParameterSet& other) const
    {
        return (security == other.security)
            && (noise_size_bits == other.noise_size_bits)
            && (private_key_size_bits == other.private_key_size_bits)
            && (ciphertext_size_bits == other.ciphertext_size_bits)
            && (oracle_seed == other.oracle_seed);
    }


    PrivateKey::PrivateKey(const ParameterSet & parameter_set) :
      _parameter_set(parameter_set)
    {
        initialize_random_generators();
        generate_values();
    }

    bool PrivateKey::operator==(const PrivateKey & other) const
    {
        return (_parameter_set == other._parameter_set)
            && (_private_element == other._private_element);
    }

    PrivateKey& PrivateKey::generate_values()
    {
        // Generate odd eta-bit integer
        do {
            _private_element = _generator->get_bits(_parameter_set.private_key_size_bits);
        } while (_private_element % 2 == 0);

        // Generate random odd q from [1, 2^gamma / p)
        const mpz_class max_gamma_bit_z = mpz_class(1) << _parameter_set.ciphertext_size_bits;
        const mpz_int q_upper_bound = mpz_int(max_gamma_bit_z.get_mpz_t())
                                   / _private_element;
        mpz_int q;
        do {
            q = _generator->get_range(q_upper_bound);
        } while (q % 2 == 0);

        return *this;
    }

    void PrivateKey::initialize_random_generators() const
    {
        _generator.reset(new CSPRNG);
        _oracle.reset(new RandomOracle{_parameter_set.ciphertext_size_bits, _parameter_set.oracle_seed});
    }

    CompressedCiphertext PrivateKey::encrypt(const std::vector<bool> & bits) const
    {
        _oracle->reset();

        CompressedCiphertext result(_parameter_set);

        // Generate compressed public element
        const mpz_int & oracle_output = _oracle->next();
        result._public_element_delta = oracle_output % _private_element;

        for (const bool m : bits)
        {
            // Choose random noise
            const mpz_int r = _generator->get_range_bits(_parameter_set.noise_size_bits) + 1;

            // Random oracle output
            const mpz_int & oracle_output = _oracle->next();

            // Add compressed ciphertext deltas
            result._elements_deltas.push_back(oracle_output % _private_element - 2*r - m);
        }

        return result;
    }

    vector<bool> PrivateKey::decrypt(const HomomorphicArray & array) const
    {
        _oracle->reset();

        vector<bool> result;
        for (const mpz_int & element : array.elements())
        {
            const mpz_int m = element % _private_element % 2;
            result.push_back(static_cast<bool>(m));
        }
        return result;
    }

} // namespace she