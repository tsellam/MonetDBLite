library(testthat)
library(dplyr)
library(nycflights13)
library(MonetDBLite)

dbdir <- file.path(tempdir(), "dplyrdir")
my_db_sqlite <- FALSE
my_db_monetdb <- FALSE
flights_sqlite <- FALSE
flights_monetdb <- FALSE


test_that("we can connect", {
	my_db_sqlite <<- src_sqlite(tempfile(), create = T)
	my_db_monetdb <<- src_monetdb(embedded=dbdir)
})

# TEMPORARY until https://github.com/hannesmuehleisen/MonetDBLite/issues/15
flights$time_hour <- as.numeric( flights$time_hour )


test_that("dplyr copy_to()", {

	flights_sqlite <<- copy_to(my_db_sqlite, flights, temporary = FALSE, indexes = list(
	  c("year", "month", "day"), "carrier", "tailnum"))

	flights_monetdb <<- copy_to(my_db_monetdb, flights, temporary = FALSE, indexes = list(
	  c("year", "month", "day"), "carrier", "tailnum"))
})




test_that("dplyr tbl( sql() )", {
	expect_equal( 
		collect(tbl(my_db_sqlite, sql("SELECT * FROM flights"))) ,
		collect(tbl(my_db_monetdb, sql("SELECT * FROM flights"))) 
	)
})


test_that("dplyr select()", {
	expect_equal(
		collect(select(flights_sqlite, year:day, dep_delay, arr_delay)) ,
		collect(select(flights_monetdb, year:day, dep_delay, arr_delay))
	)
})


test_that("dplyr filter()", {
	expect_equal(
		collect(filter(flights_sqlite, dep_delay > 240)) ,
		collect(filter(flights_monetdb, dep_delay > 240))
	)
})


test_that("dplyr arrange()", {
	expect_equal(
		collect(arrange(flights_sqlite, year, month, day, dep_time)) ,
		collect(arrange(flights_monetdb, year, month, day, dep_time))
	)
})


test_that("dplyr mutate()", {
	expect_equal(
		collect(mutate(flights_sqlite, speed = air_time / distance)) ,
		collect(mutate(flights_monetdb, speed = air_time / distance))
	)
})


test_that("dplyr summarise()", {
	expect_equal(
		data.frame(summarise(flights_sqlite, delay = mean(dep_time))),
		data.frame(summarise(flights_monetdb, delay = mean(dep_time)))
	)
})



test_that("dplyr multiple objects", {
	expect_equal(
		collect(c1_sqlite <- filter(flights_sqlite, year == 2013, month == 1, day == 1)),
		collect(c1_monetdb <- filter(flights_monetdb, year == 2013, month == 1, day == 1))
	)

	expect_equal(
		collect(c2_sqlite <- select(c1_sqlite, year, month, day, carrier, dep_delay, air_time, distance)),
		collect(c2_monetdb <- select(c1_monetdb, year, month, day, carrier, dep_delay, air_time, distance))
	)

	expect_equal(
		collect(c3_sqlite <- mutate(c2_sqlite, speed = distance / air_time * 60)),
		collect(c3_monetdb <- mutate(c2_monetdb, speed = distance / air_time * 60))
	)

	expect_equal(
		collect(c4_sqlite <- arrange(c3_sqlite, year, month, day, carrier)),
		collect(c4_monetdb <- arrange(c3_monetdb, year, month, day, carrier))
	)

	expect_equal(
		collect(collect(c4_sqlite)),
		collect(collect(c4_monetdb))
	)
})


# FAILS
# test_that("dplyr copy_to", {
	# expect_equal(
		# explain(c4_sqlite),
		# explain(c4_monetdb)
	# )


test_that("dplyr group_by", {
	expect_equal(
		collect(by_tailnum_sqlite <- group_by(flights_sqlite, tailnum)),
		collect(by_tailnum_monetdb <- group_by(flights_monetdb, tailnum))
	)
})

test_that("shutdown", {
	DBI::dbDisconnect(my_db_monetdb$con, shutdown=TRUE)
})


# # # # # # # # FAILS # # # # # # # #

	# delay_sqlite <- summarise(by_tailnum_sqlite,
	  # count = n(),
	  # dist = mean(distance),
	  # delay = mean(arr_delay)
	# )


	# delay_monetdb <- summarise(by_tailnum_monetdb,
	  # count = n(),
	  # dist = mean(distance),
	  # delay = mean(arr_delay)
	# )

	# expect_equal(
		# collect(delay_sqlite),
		# collect(delay_monetdb)
	# )


	# delay_sqlite <- filter(delay_sqlite, count > 20, dist < 2000)
	# delay_monetdb <- filter(delay_monetdb, count > 20, dist < 2000)

	# expect_equal(
		# collect(delay_sqlite),
		# collect(delay_monetdb)
	# )


	# delay_local_sqlite <- collect(delay_sqlite)
	# delay_local_monetdb <- collect(delay_monetdb)

	# expect_equal(
		# collect(delay_local_sqlite),
		# collect(delay_local_monetdb)
	# )

	# daily_sqlite <- group_by(flights_sqlite, year, month, day)
	# daily_monetdb <- group_by(flights_monetdb, year, month, day)

	# bestworst_sqlite <- daily_sqlite %>% 
	  # select(flight, arr_delay) %>% 
	  # filter(arr_delay == min(arr_delay) || arr_delay == max(arr_delay))

	# bestworst_monetdb <- daily_monetdb %>% 
	  # select(flight, arr_delay) %>% 
	  # filter(arr_delay == min(arr_delay) || arr_delay == max(arr_delay))

	# expect_equal( bestworst_sqlite$query , bestworst_monetdb$query )

	# expect_equal(
		# collect(bestworst_sqlite),
		# collect(bestworst_monetdb)
	# )



	# ranked_sqlite <- daily_sqlite %>% 
	  # select(arr_delay) %>% 
	  # mutate(rank = rank(desc(arr_delay)))



	# ranked_monetdb <- daily_monetdb %>% 
	  # select(arr_delay) %>% 
	  # mutate(rank = rank(desc(arr_delay)))


	# expect_equal( ranked_sqlite$query , ranked_monetdb$query )

	# expect_equal(
		# collect(ranked_sqlite),
		# collect(ranked_monetdb)
	# )


