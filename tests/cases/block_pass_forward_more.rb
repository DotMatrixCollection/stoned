def forward(*args, &blk)
  blk.call(*args)
end

p forward(2, 3) { |a, b| a * b }
