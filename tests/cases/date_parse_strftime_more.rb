require "date"

d = Date.parse("2026-06-01")
p d.strftime("%Y")
p d.strftime("%m/%d/%Y")
p d.wday
