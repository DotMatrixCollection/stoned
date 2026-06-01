t = Thread.new { 21 * 2 }
p t.alive?
p t.join.class
p t.value
p t.status
