require "bundler"

module Bundler
  module Source
    class Metadata
      def to_s
        "metadata"
      end
    end
  end
end
