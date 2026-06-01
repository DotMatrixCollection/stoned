seen = []
at_exit { seen << :first; p seen }
at_exit { seen << :second }
seen << :body
