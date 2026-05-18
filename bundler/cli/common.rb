require "bundler"

module Bundler
  class CLI
    module Common
      class << self
        def output_without_groups
          Bundler.settings["WITHOUT"]
        end
      end
    end
  end
end
