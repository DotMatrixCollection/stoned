t = Time.now
p t.class
p t.year > 2020
p t.month.between?(1, 12)
p t.day.between?(1, 31)
p t.hour.between?(0, 23)
p t.min.between?(0, 59)
p t.sec.between?(0, 60)
p t.to_i > 0
p t.strftime("%Y").length == 4
