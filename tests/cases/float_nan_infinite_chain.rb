p (0.0 / 0.0).nan?
p (1.0 / 0.0).infinite?
p (-1.0 / 0.0).infinite?
p 1.5.finite?
p Float::INFINITY.infinite?
p [1.0, Float::INFINITY, -Float::INFINITY, Float::NAN].map(&:finite?)
