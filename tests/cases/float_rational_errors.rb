p 1.25.to_r.to_s
p (1.0 / 3.0).rationalize.to_s
begin
  (0.0 / 0.0).to_r
rescue FloatDomainError => e
  puts e.class
end
