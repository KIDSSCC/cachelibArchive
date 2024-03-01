./Build/tmdb_test -f tmdb_1 -r 2 > tmdb_1.res 2>&1 &
./Build/tmdb_test -f tmdb_2 -r 7 > tmdb_2.res 2>&1 &

wait

