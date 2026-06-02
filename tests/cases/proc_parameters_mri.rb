# Non-lambda procs: positional required params reported as :opt
pr = proc { |a, b| a + b }
p pr.lambda?
p pr.parameters

# Lambda: positional required params reported as :req
lam = lambda { |a, b| a + b }
p lam.lambda?
p lam.parameters

# Arrow lambda same as lambda
arr = ->(a, b) { a + b }
p arr.lambda?
p arr.parameters

# Proc with keyword required: still :keyreq
kw_pr = proc { |a, b:, c: 1| [a, b, c] }
p kw_pr.parameters

# Mixed: proc with splat and block
mix = proc { |a, *rest, &blk| }
p mix.parameters

# No-param proc/lambda
p proc { }.parameters
p lambda { }.parameters
