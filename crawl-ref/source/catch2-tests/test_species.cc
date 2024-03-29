#include "catch_amalgamated.hpp"

#include "AppHdr.h"
#include "species.h"

TEST_CASE( "Random draconian species are not removed", "[single-file]" ) {
    for (int i = 0; i < 100; ++i)
    {
        auto species = species::random_draconian_colour();
        REQUIRE( !species::is_removed(species) );
    }
}

TEST_CASE( "Random draconian species are derived draconians", "[single-file]" ) {
    for (int i = 0; i < 100; ++i)
    {
        auto species = species::random_draconian_colour();
        REQUIRE( species::is_draconian(species) );
        REQUIRE( species != SP_BASE_DRACONIAN );
    }
}
