PointMore = Struct.new(:x, :y, :z)
p PointMore.new(1, 2, 3).select { |v| v.odd? }
p PointMore.members
